#pragma once

#include <vulkan/vulkan_core.h>

#include <vector>

#include "deletion_queue.h"
#include "swapchain.h"

namespace tr::renderer {
struct Renderpass {
  static auto init(VkDevice device, tr::renderer::Swapchain& swapchain)
      -> Renderpass;

  void defer_deletion(DeviceDeletionStack& device_deletion_stack) {
    for (auto framebuffer : framebuffers) {
      device_deletion_stack.defer_deletion(DeviceHandle::Framebuffer,
                                           framebuffer);
    }

    device_deletion_stack.defer_deletion(DeviceHandle::RenderPass,
                                         vk_renderpass);
  }

  VkRenderPass vk_renderpass = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> framebuffers;
};
}  // namespace tr::renderer
