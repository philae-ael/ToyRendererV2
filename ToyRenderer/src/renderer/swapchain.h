#pragma once

#include <utils/types.h>
#include <vulkan/vulkan_core.h>

#include <vector>

#include "../options.h"
#include "deletion_queue.h"
#include "device.h"
#include "surface.h"
#include "utils/cast.h"

namespace tr::renderer {

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

struct FrameSynchroPool {
  // this may be awful, idk yet
  auto pop(VkDevice device) -> FrameSynchro {
    if (frame_synchronisation_storage.empty()) {
      auto synchro = FrameSynchro::init(device);
      return synchro;
    }

    auto synchro = frame_synchronisation_storage.front();
    frame_synchronisation_storage.pop_front();

    return synchro;
  }
  void push_frame_synchro(DeviceDeletionStack &device_deletion_stack, FrameSynchro synchro) {
    if (frame_synchronisation_storage.size() > 5) {
      synchro.defer_deletion(device_deletion_stack);
    } else {
      frame_synchronisation_storage.push_back(synchro);
    }
  }

  void cleanup(DeviceDeletionStack &device_deletion_stack) {
    for (auto synchro : frame_synchronisation_storage) {
      synchro.defer_deletion(device_deletion_stack);
    }
    frame_synchronisation_storage.clear();
  }

 private:
  std::deque<FrameSynchro> frame_synchronisation_storage;
};

struct Frame {
  std::uint32_t swapchain_image_index = 0;
  FrameSynchro synchro;

  auto submitCmds(Device &device, std::span<VkCommandBuffer> cmds) const -> VkResult {
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &synchro.present_semaphore,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = utils::narrow_cast<std::uint32_t>(cmds.size()),
        .pCommandBuffers = cmds.data(),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &synchro.render_semaphore,
    };

    return vkQueueSubmit(device.queues.graphics_queue, 1, &submit, synchro.render_fence);
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
  static auto init(tr::Options &options, Device &device, VkSurfaceKHR surface, GLFWwindow *window) -> Swapchain;
  static auto init_with_config(SwapchainConfig config, Device &device, VkSurfaceKHR surface, GLFWwindow *window)
      -> Swapchain;

  void defer_deletion(DeviceDeletionStack &device_deletion_stack) {
    for (auto view : image_views) {
      device_deletion_stack.defer_deletion(DeviceHandle::ImageView, view);
    }
    device_deletion_stack.defer_deletion(DeviceHandle::SwapchainKHR, vk_swapchain);
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

  struct SwapchainConfig {
    VkPresentModeKHR prefered_present_mode;
  } config;

 private:
  auto compute_extent(GLFWwindow *window) const -> VkExtent2D;
};
}  // namespace tr::renderer
