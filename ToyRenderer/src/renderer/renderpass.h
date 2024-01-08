#pragma once

#include <vulkan/vulkan_core.h>

#include <vector>

#include "deletion_queue.h"
#include "swapchain.h"
#include "utils/cast.h"
#include "utils/misc.h"

namespace tr::renderer {
struct Renderpass {
  static auto init(VkDevice device, tr::renderer::Swapchain& swapchain) -> Renderpass;

  void defer_deletion(DeviceDeletionStack& device_deletion_stack) {
    for (auto framebuffer : framebuffers) {
      device_deletion_stack.defer_deletion(DeviceHandle::Framebuffer, framebuffer);
    }

    device_deletion_stack.defer_deletion(DeviceHandle::RenderPass, vk_renderpass);
  }

  VkRenderPass vk_renderpass = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> framebuffers;

  void begin(VkCommandBuffer cmd, uint32_t swapchain_image_index, VkRect2D render_area,
             std::span<const VkClearValue> clear_values) const {
    VkRenderPassBeginInfo rp_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = vk_renderpass,
        .framebuffer = framebuffers[swapchain_image_index],
        .renderArea = render_area,
        .clearValueCount = utils::narrow_cast<uint32_t>(clear_values.size()),
        .pClearValues = clear_values.data(),
    };

    vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
  }
  void end(VkCommandBuffer cmd) const {
    utils::ignore_unused(this);
    vkCmdEndRenderPass(cmd);
  }
};
}  // namespace tr::renderer
