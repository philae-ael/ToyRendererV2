#pragma once

#include <vulkan/vulkan_core.h>

#include <array>

#include "../debug.h"
#include "../descriptors.h"
#include "../ressources.h"
#include "utils/cast.h"

namespace tr::renderer {

struct Deferred {
  std::array<VkDescriptorSetLayout, 1> descriptor_set_layouts{};
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  static constexpr std::array bindings = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(),
      DescriptorSetLayoutBindingBuilder{}
          .binding_(1)
          .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(),
  });
  static constexpr std::array attachments_definitions = utils::to_array<ImageDefinition>({
      {
          // swapchain
          .flags = IMAGE_OPTION_FLAG_FORMAT_SAME_AS_FRAMEBUFFER_BIT | IMAGE_OPTION_FLAG_SIZE_SAME_AS_FRAMEBUFFER_BIT,
          .usage = IMAGE_USAGE_COLOR_BIT,
      },
  });

  static auto init(VkDevice &device, Swapchain &swapchain, DeviceDeletionStack &device_deletion_stack) -> Deferred;
  void defer_deletion(DeviceDeletionStack &device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::Pipeline, pipeline);
    device_deletion_stack.defer_deletion(DeviceHandle::PipelineLayout, pipeline_layout);
    for (auto descriptor_set_layout : descriptor_set_layouts) {
      device_deletion_stack.defer_deletion(DeviceHandle::DescriptorSetLayout, descriptor_set_layout);
    }
  }

  template <class Fn>
  void draw(VkCommandBuffer cmd, FrameRessourceManager &rm, VkRect2D render_area, Fn f) const {
    DebugCmdScope scope(cmd, "Deferred");

    std::array<VkRenderingAttachmentInfo, 1> attachments{
        rm.swapchain.invalidate()
            .sync(cmd, SyncColorAttachment)
            .as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 1.0, 1.0}}}),
    };

VkRenderingInfo render_info{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = render_area,
    .layerCount = 1,
    .viewMask = 0,
        .colorAttachmentCount = attachments.size(),
        .pColorAttachments = attachments.data(),
        .pDepthAttachment = nullptr,
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
