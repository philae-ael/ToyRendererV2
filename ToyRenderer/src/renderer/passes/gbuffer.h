#pragma once

#include <vulkan/vulkan_core.h>

#include <array>

#include "../debug.h"
#include "../descriptors.h"
#include "../ressources.h"
#include "utils/cast.h"

namespace tr::renderer {

struct GBuffer {
  std::array<VkDescriptorSetLayout, 2> descriptor_set_layouts{};
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  static constexpr std::array set_0 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_VERTEX_BIT)
          .build(),
  });

  static constexpr std::array set_1 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
          .descriptor_count(2)
          .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(),
  });

  static constexpr std::array definitions = utils::to_array<ImageDefinition>({
      {
          // RGB: color, A: Roughness
          .flags = 0,
          .usage = IMAGE_USAGE_COLOR_BIT | IMAGE_USAGE_SAMPLED_BIT, // NOTE: They should NOT be sampled !
          .size = FrameBufferExtent{},
          .format = VK_FORMAT_R8G8B8A8_UNORM,
      },
      {
          // RGB: normal, A: metallic
          .flags = 0,
          .usage = IMAGE_USAGE_COLOR_BIT | IMAGE_USAGE_SAMPLED_BIT,
          .size = FrameBufferExtent{},
          .format = VK_FORMAT_R8G8B8A8_SNORM,
      },
      {
          // RGB: ViewDir
          .flags = 0,
          .usage = IMAGE_USAGE_COLOR_BIT | IMAGE_USAGE_SAMPLED_BIT,
          .size = FrameBufferExtent{},
          .format = VK_FORMAT_R8G8B8A8_SNORM,
      },
      {
          // depth
          .flags = 0,
          .usage = IMAGE_USAGE_DEPTH_BIT,
          .size = FrameBufferExtent{},
          .format = VK_FORMAT_D16_UNORM,
      },
  });

  static auto init(VkDevice &device, Swapchain &Swapchain, DeviceDeletionStack &device_deletion_stack) -> GBuffer;
  void defer_deletion(DeviceDeletionStack &device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::Pipeline, pipeline);
    device_deletion_stack.defer_deletion(DeviceHandle::PipelineLayout, pipeline_layout);
    for (auto descriptor_set_layout : descriptor_set_layouts) {
      device_deletion_stack.defer_deletion(DeviceHandle::DescriptorSetLayout, descriptor_set_layout);
    }
  }

  template <class Fn>
  void draw(VkCommandBuffer cmd, FrameRessourceManager &rm, VkRect2D render_area, Fn f) {
    DebugCmdScope scope(cmd, "GBuffer");

    ImageMemoryBarrier::submit<4>(cmd, {{
                                           rm.fb0.invalidate().prepare_barrier(SyncColorAttachment),
                                           rm.fb1.invalidate().prepare_barrier(SyncColorAttachment),
                                           rm.fb2.invalidate().prepare_barrier(SyncColorAttachment),
                                           rm.depth.invalidate().prepare_barrier(SyncLateDepth),
                                       }});

    std::array attachments = utils::to_array<VkRenderingAttachmentInfo>({
        rm.fb0.as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
        rm.fb1.as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
        rm.fb2.as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
    });

    VkRenderingAttachmentInfo depthAttachment =
        rm.depth.as_attachment(VkClearValue{.depthStencil = {.depth = 1., .stencil = 0}});

    VkRenderingInfo render_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = render_area,
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = utils::narrow_cast<uint32_t>(attachments.size()),
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
