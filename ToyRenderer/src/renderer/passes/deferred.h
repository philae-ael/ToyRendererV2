#pragma once

#include <vulkan/vulkan_core.h>

#include "../debug.h"
#include "../pipeline.h"
#include "../ressources.h"

namespace tr::renderer {

struct Deferred {
  Pipeline pipeline;

  static auto init(Device &device) -> Deferred { return {Pipeline::init(device)}; }
  void defer_deletion(DeviceDeletionStack &device_deletion_stack) const {
    pipeline.defer_deletion(device_deletion_stack);
  }

  template <class Fn>
  void draw(VkCommandBuffer cmd, FrameRessourceManager &rm, VkRect2D render_area, Fn f) const {
    DebugCmdScope scope(cmd, "GBuffer");

    std::array<VkRenderingAttachmentInfo, 1> attachments{
        rm.swapchain.transition(cmd, DstImageMemoryBarrierColorAttachment)
            .as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 1.0, 1.0}}}),
        /* rm.fb1.transition(cmd, DstImageMemoryBarrierColorAttachment) */
        /*     .as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 1.0, 1.0}}}), */
    };

    VkRenderingAttachmentInfo depthAttachment =
        rm.depth.transition(cmd, DstImageMemoryBarrierDepthAttachment)
            .as_attachment(VkClearValue{.depthStencil = {.depth = 1., .stencil = 0}});

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
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.vk_pipeline);
    f();
    vkCmdEndRendering(cmd);
  }
};

}  // namespace tr::renderer
