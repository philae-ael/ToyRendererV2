#pragma once

#include <utils/types.h>
#include <vulkan/vulkan_core.h>

#include <vector>

#include "deletion_queue.h"
#include "device.h"
#include "surface.h"

namespace tr::renderer {
struct FrameSynchro {
  static auto init(VkDevice device) -> FrameSynchro;
  void defer_deletion(DeviceDeletionStack &device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::Fence, render_fence);
    device_deletion_stack.defer_deletion(DeviceHandle::Semaphore,
                                         render_semaphore);
    device_deletion_stack.defer_deletion(DeviceHandle::Semaphore,
                                         present_semaphore);
  }
  VkFence render_fence = VK_NULL_HANDLE;
  VkSemaphore render_semaphore = VK_NULL_HANDLE;
  VkSemaphore present_semaphore = VK_NULL_HANDLE;
};

struct Frame {
  std::uint32_t swapchain_image_index = 0;
  FrameSynchro synchro;
};

struct Swapchain {
  static auto init(Device &device, VkSurfaceKHR surface, GLFWwindow *window)
      -> Swapchain;

  void defer_deletion(DeviceDeletionStack &device_deletion_stack) {
    for (auto view : image_views) {
      device_deletion_stack.defer_deletion(DeviceHandle::ImageView, view);
    }
    device_deletion_stack.defer_deletion(DeviceHandle::SwapchainKHR,
                                         vk_swapchain);
  }

  auto acquire_next_frame(Device &, FrameSynchro) const -> Frame;

  VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;

  VkExtent2D extent{};
  VkSurfaceFormatKHR surface_format{};
  VkPresentModeKHR present_mode{};
  VkSurfaceCapabilitiesKHR capabilities{};

  std::vector<VkSurfaceFormatKHR> available_formats;
  std::vector<VkPresentModeKHR> available_present_modes;

  std::vector<VkImage> images;
  std::vector<VkImageView> image_views;

 private:
  auto compute_extent(GLFWwindow *window) const -> VkExtent2D;
};
}  // namespace tr::renderer
