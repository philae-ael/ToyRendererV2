#pragma once

#include <utils/types.h>
#include <vulkan/vulkan_core.h>

#include <vector>

#include "deletion_queue.h"
#include "descriptors.h"
#include "device.h"
#include "queue.h"
#include "surface.h"
#include "utils.h"

namespace tr::renderer {

// TODO: MOVE THAT
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

struct Swapchain {
  struct SwapchainConfig;
  void reinit(Device &device, VkSurfaceKHR surface, GLFWwindow *window) {
    *this = init_with_config(config, device, surface, window);
  }
  static auto init_with_config(SwapchainConfig config, Device &device, VkSurfaceKHR surface, GLFWwindow *window)
      -> Swapchain;

  void defer_deletion(DeviceDeletionStack &device_deletion_stack) {
    for (auto view : image_views) {
      device_deletion_stack.defer_deletion(DeviceHandle::ImageView, view);
    }
    device_deletion_stack.defer_deletion(DeviceHandle::SwapchainKHR, vk_swapchain);
  }

  auto acquire_next_frame(tr::renderer::Device &device, Frame *frame) const -> VkResult;

  VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;

  VkExtent2D extent{};
  VkSurfaceFormatKHR surface_format{};
  VkPresentModeKHR present_mode{};
  VkSurfaceCapabilitiesKHR capabilities{};

  std::vector<VkSurfaceFormatKHR> available_formats;
  std::vector<VkPresentModeKHR> available_present_modes;

  std::vector<VkImage> images;
  std::vector<VkImageView> image_views;

  struct SwapchainConfig {
    VkPresentModeKHR prefered_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
  } config;

 private:
  auto compute_extent(GLFWwindow *window) const -> VkExtent2D;
};
}  // namespace tr::renderer
