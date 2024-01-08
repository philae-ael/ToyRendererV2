#pragma once

#include "descriptors.h"
#include "device.h"
#include "queue.h"
#include "ressources.h"
#include "utils.h"

namespace tr::renderer {

struct OneTimeCommandBuffer {
  VkCommandBuffer vk_cmd = VK_NULL_HANDLE;

  [[nodiscard]] auto begin() const -> VkResult {
    VkCommandBufferBeginInfo cmd_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    return vkBeginCommandBuffer(vk_cmd, &cmd_begin_info);
  }

  [[nodiscard]] auto end() const -> VkResult { return vkEndCommandBuffer(vk_cmd); }

  static auto allocate(VkDevice device, VkCommandPool command_pool) -> OneTimeCommandBuffer {
    OneTimeCommandBuffer cmd;
    VkCommandBufferAllocateInfo transfer_cmd_alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VK_UNWRAP(vkAllocateCommandBuffers, device, &transfer_cmd_alloc_info, &cmd.vk_cmd);
    return cmd;
  }
};

struct FrameSynchro {
  static auto init(VkDevice device) -> FrameSynchro;
  void defer_deletion(DeviceDeletionStack &device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::Fence, render_fence);
    device_deletion_stack.defer_deletion(DeviceHandle::Semaphore, render_semaphore);
    device_deletion_stack.defer_deletion(DeviceHandle::Semaphore, present_semaphore);
  }
  VkFence render_fence = VK_NULL_HANDLE;
  VkSemaphore render_semaphore = VK_NULL_HANDLE;
  VkSemaphore present_semaphore = VK_NULL_HANDLE;
};

struct Frame {
  std::uint32_t swapchain_image_index = 0;
  FrameSynchro synchro;
  OneTimeCommandBuffer cmd{};
  DescriptorAllocator descriptor_allocator{};
  FrameRessourceManager frm{};

  auto submitCmds(VkQueue queue) const -> VkResult {
    return QueueSubmit{}
        .wait_semaphores<1>({{synchro.present_semaphore}}, {{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}})
        .signal_semaphores({{synchro.render_semaphore}})
        .command_buffers({{cmd.vk_cmd}})
        .submit(queue, synchro.render_fence);
  }

  auto present(Device &device, VkSwapchainKHR swapchain) const -> VkResult {
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &synchro.render_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &swapchain_image_index,
        .pResults = nullptr,
    };
    return vkQueuePresentKHR(device.queues.present_queue, &present_info);
  }
};
}  // namespace tr::renderer
