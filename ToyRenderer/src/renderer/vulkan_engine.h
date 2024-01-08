#pragma once

#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <vulkan/vulkan_core.h>

#include <deque>

#include "command_pool.h"
#include "debug.h"
#include "deletion_queue.h"
#include "device.h"
#include "instance.h"
#include "renderpass.h"
#include "surface.h"
#include "swapchain.h"
#include "timestamp.h"
#include "utils.h"

namespace tr::renderer {
class VulkanEngine {
 public:
  VulkanEngine() = default;
  void init(tr::Options& options,
            std::span<const char*> required_instance_extensions,
            GLFWwindow* window) {
    if (options.debug.renderdoc) {
      renderdoc = tr::renderer::Renderdoc::init();
    }

    this->window = window;
    instance = Instance::init(options, required_instance_extensions);
    instance.defer_deletion(global_deletion_stacks.instance);

    surface = Surface::init(instance.vk_instance, window);
    Surface::defer_deletion(surface, global_deletion_stacks.instance);

    device = Device::init(instance.vk_instance, surface);
    device.defer_deletion(global_deletion_stacks.instance);

    swapchain = Swapchain::init(device, surface, window);
    swapchain.defer_deletion(swapchain_device_deletion_stack);

    renderpass = tr::renderer::Renderpass::init(device.vk_device, swapchain);
    renderpass.defer_deletion(swapchain_device_deletion_stack);

    graphics_command_pool =
        CommandPool::init(device, CommandPool::TargetQueue::Graphics);
    CommandPool::defer_deletion(graphics_command_pool,
                                global_deletion_stacks.device);

    timestamp = Timestamp<1, 2>::init(device);

    VkCommandBufferAllocateInfo cmd_alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VK_UNWRAP(vkAllocateCommandBuffers, device.vk_device, &cmd_alloc_info,
              &main_command_buffer);
  }

  void sync() const {
    VK_UNWRAP(vkQueueWaitIdle, device.queues.graphics_queue);
    VK_UNWRAP(vkQueueWaitIdle, device.queues.present_queue);
  }

  void rebuild_swapchain() {
    sync();
    swapchain_device_deletion_stack.cleanup(device.vk_device);

    swapchain = Swapchain::init(device, surface, window);
    swapchain.defer_deletion(swapchain_device_deletion_stack);

    renderpass = tr::renderer::Renderpass::init(device.vk_device, swapchain);
    renderpass.defer_deletion(swapchain_device_deletion_stack);
  }

  void draw() {
    Frame frame = swapchain.acquire_next_frame(device, pop_frame_synchro());
    /* timestamp.reset(device.vk_device); */

    VK_UNWRAP(vkResetCommandBuffer, main_command_buffer, 0);

    VkCommandBuffer cmd = main_command_buffer;
    VkCommandBufferBeginInfo cmd_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    VK_UNWRAP(vkBeginCommandBuffer, cmd, &cmd_begin_info);
    {
      DebugCmdScope scope{cmd, "test?"};
      timestamp.write_cmd_query(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);

      VkClearValue clearValue{
          .color =
              {
                  .float32 = {1.0, 1.0, 1.0, 0.0},
              },
      };
      VkRenderPassBeginInfo rp_info{
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          .pNext = nullptr,
          .renderPass = renderpass.vk_renderpass,
          .framebuffer = renderpass.framebuffers[frame.swapchain_image_index],
          .renderArea = {.offset = {0, 0}, .extent = swapchain.extent},
          .clearValueCount = 1,
          .pClearValues = &clearValue,
      };

      vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdEndRenderPass(cmd);

      timestamp.write_cmd_query(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1);
    }

    VK_UNWRAP(vkEndCommandBuffer, cmd);

    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.synchro.present_semaphore,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frame.synchro.render_semaphore,
    };

    VK_UNWRAP(vkQueueSubmit, device.queues.graphics_queue, 1, &submit,
              frame.synchro.render_fence);

    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.synchro.render_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain.vk_swapchain,
        .pImageIndices = &frame.swapchain_image_index,
        .pResults = nullptr,
    };

    VkResult result =
        vkQueuePresentKHR(device.queues.present_queue, &present_info);

    switch (result) {
      case VkResult::VK_ERROR_OUT_OF_DATE_KHR:
        rebuild_swapchain();
        break;
      default:
        TR_ASSERT(result == VkResult::VK_SUCCESS,
                  "error while calling vkQueuePresentKHR got error code {}",
                  result);
        break;
    }
    frame_device_deletion_stack.cleanup(device.vk_device);
    push_frame_synchro(frame.synchro);

    timestamp.get(device.vk_device);

    spdlog::trace("Took {}ms",
                  timestamp.fetch_timestamp(1) - timestamp.fetch_timestamp(0));
  }

  ~VulkanEngine() {
    sync();
    for (auto synchro : frame_synchronisation_storage) {
      synchro.defer_deletion(global_deletion_stacks.device);
    }

    // TODO: add a free queue for buffers?
    vkFreeCommandBuffers(device.vk_device, graphics_command_pool, 1,
                         &main_command_buffer);
    swapchain_device_deletion_stack.cleanup(device.vk_device);
    global_deletion_stacks.device.cleanup(device.vk_device);
    global_deletion_stacks.instance.cleanup(instance.vk_instance);
    vkDestroyInstance(instance.vk_instance, nullptr);
  }

  VulkanEngine(const VulkanEngine&) = delete;
  VulkanEngine(VulkanEngine&&) = delete;
  auto operator=(const VulkanEngine&) -> VulkanEngine& = delete;
  auto operator=(VulkanEngine&&) -> VulkanEngine& = delete;

 private:
  GLFWwindow* window{};
  struct DeletionStacks {
    InstanceDeletionStack instance;
    DeviceDeletionStack device;
  };

  // Cleaned at exit
  DeletionStacks global_deletion_stacks;

  // Cleaned on swapchain recreation
  DeviceDeletionStack swapchain_device_deletion_stack;

  // Cleaned every frame
  DeviceDeletionStack frame_device_deletion_stack;

  Renderdoc renderdoc;
  Instance instance;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  Device device;
  Swapchain swapchain;

  //
  //

  VkCommandPool graphics_command_pool = VK_NULL_HANDLE;
  Timestamp<1, 2> timestamp;

  Renderpass renderpass;
  VkCommandBuffer main_command_buffer = VK_NULL_HANDLE;

  // this may be awful, idk yet
  auto pop_frame_synchro() -> FrameSynchro {
    if (frame_synchronisation_storage.empty()) {
      auto synchro = FrameSynchro::init(device.vk_device);
      return synchro;
    }

    auto synchro = frame_synchronisation_storage.front();
    frame_synchronisation_storage.pop_front();

    return synchro;
  }
  void push_frame_synchro(FrameSynchro synchro) {
    if (frame_synchronisation_storage.size() > 5) {
      synchro.defer_deletion(frame_device_deletion_stack);
    } else {
      frame_synchronisation_storage.push_back(synchro);
    }
  }
  std::deque<FrameSynchro> frame_synchronisation_storage;
};

}  // namespace tr::renderer
