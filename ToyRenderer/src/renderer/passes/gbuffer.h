#pragma once

#include <vulkan/vulkan_core.h>

#include <array>

#include "../debug.h"
#include "../ressources.h"
#include "utils/cast.h"

namespace tr::renderer {

struct GBuffer {
  VkPipeline pipeline = VK_NULL_HANDLE;

  static constexpr std::array definitions = utils::to_array<ImageDefinition>({
      {
          // fb0
          .flags = IMAGE_OPTION_FLAG_FORMAT_B8G8R8A8_UNORM_BIT | IMAGE_OPTION_FLAG_SIZE_SAME_AS_FRAMEBUFFER_BIT,
          .usage = ImageUsage::Color,
      },
      {
          // fb1
          .flags = IMAGE_OPTION_FLAG_FORMAT_B8G8R8A8_UNORM_BIT | IMAGE_OPTION_FLAG_SIZE_SAME_AS_FRAMEBUFFER_BIT,
          .usage = ImageUsage::Color,
      },
      {
          // depth
          .flags = IMAGE_OPTION_FLAG_FORMAT_D16_UNORM_BIT | IMAGE_OPTION_FLAG_SIZE_SAME_AS_FRAMEBUFFER_BIT,
          .usage = ImageUsage::Depth,
      },
  });

  static auto init(VkDevice &device, Swapchain &Swapchain, DeviceDeletionStack &device_deletion_stack) -> GBuffer;
  void defer_deletion(DeviceDeletionStack &device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::Pipeline, pipeline);
  }

  template <class Fn>
  void draw(VkCommandBuffer cmd, FrameRessourceManager &rm, VkRect2D render_area, Fn f) {
    DebugCmdScope scope(cmd, "GBuffer");

    std::array attachments = utils::to_array<VkRenderingAttachmentInfo>({
        rm.fb0.invalidate()
            .sync(cmd, SyncColorAttachment)
            .as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
        rm.fb1.invalidate()
            .sync(cmd, SyncColorAttachment)
            .as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
    });

    VkRenderingAttachmentInfo depthAttachment =
        rm.depth.sync(cmd, SyncLateDepth).as_attachment(VkClearValue{.depthStencil = {.depth = 1., .stencil = 0}});

    VkRenderingInfo render_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = render_area,
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = attachments.size(),
        .pColorAttachments = attachments.data(),
        .pDepthAttachment = &depthAttachment,
        .pStencilAttachment = nullptr,
    };
    vkCmdBeginRendering(cmd, &render_info);

    VkViewport viewport{
        0, 0, static_cast<float>(render_area.extent.width), static_cast<float>(render_area.extent.height), 0.0, 1.0,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &render_area);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    f();

    vkCmdEndRendering(cmd);
  }
};

}  // namespace tr::renderer
