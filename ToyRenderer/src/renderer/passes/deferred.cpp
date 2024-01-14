#include "deferred.h"

#include <vulkan/vulkan_core.h>

#include <array>

#include "../debug.h"
#include "../descriptors.h"
#include "../frame.h"
#include "../mesh.h"
#include "../pipeline.h"
#include "../vulkan_engine.h"

const std::array triangle_vert_bin = std::to_array<uint32_t>({
#include "shaders/deferred.vert.inc"
});

const std::array triangle_frag_bin = std::to_array<uint32_t>({
#include "shaders/deferred.frag.inc"
});

auto tr::renderer::Deferred::init(VkDevice &device, Swapchain &swapchain, DeviceDeletionStack &device_deletion_stack)
    -> Deferred {
  const auto frag = Shader::init_from_src(device, triangle_frag_bin);
  const auto vert = Shader::init_from_src(device, triangle_vert_bin);
  frag.defer_deletion(device_deletion_stack);
  vert.defer_deletion(device_deletion_stack);

  const auto shader_stages = std::to_array({
      frag.pipeline_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, "main"),
      vert.pipeline_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, "main"),
  });

  const std::array<VkDynamicState, 2> dynamic_states = {
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VIEWPORT,
  };
  const auto dynamic_state_state = PipelineDynamicStateBuilder{}.dynamic_state(dynamic_states).build();

  const auto vertex_input_state = PipelineVertexInputStateBuilder{}.build();
  const auto input_assembly_state =
      PipelineInputAssemblyBuilder{}.topology_(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST).build();
  const auto viewport_state = PipelineViewportStateBuilder{}.viewports_count(1).scissors_count(1).build();
  const auto rasterizer_state = PipelineRasterizationStateBuilder{}.build();
  const auto multisampling_state = PipelineMultisampleStateBuilder{}.build();
  const auto depth_state = DepthStateTestAndWriteOpLess.build();

  const auto color_blend_attchment_states = std::to_array<VkPipelineColorBlendAttachmentState>({
      PipelineColorBlendStateAllColorNoBlend.build(),
  });

  const auto color_formats = std::to_array<VkFormat>({
      attachments_color[0].definition.vk_format(swapchain),
  });

  const auto color_blend_state = PipelineColorBlendStateBuilder{}.attachments(color_blend_attchment_states).build();
  auto pipeline_rendering_create_info = PipelineRenderingBuilder{}.color_attachment_formats(color_formats).build();

  const auto descriptor_set_layouts = std::to_array({
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::Deferred::bindings).build(device),
  });
  const auto push_constant_ranges = std::to_array<VkPushConstantRange>({
      {
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(DirectionalLight),
      },
  });
  const auto layout = PipelineLayoutBuilder{}
                          .set_layouts(descriptor_set_layouts)
                          .push_constant_ranges(push_constant_ranges)
                          .build(device);
  VkPipeline pipeline = PipelineBuilder{}
                            .stages(shader_stages)
                            .layout_(layout)
                            .pipeline_rendering_create_info(&pipeline_rendering_create_info)
                            .vertex_input_state(&vertex_input_state)
                            .input_assembly_state(&input_assembly_state)
                            .viewport_state(&viewport_state)
                            .rasterization_state(&rasterizer_state)
                            .multisample_state(&multisampling_state)
                            .depth_stencil_state(&depth_state)
                            .color_blend_state(&color_blend_state)
                            .dynamic_state(&dynamic_state_state)
                            .build(device);

  return {descriptor_set_layouts, layout, pipeline};
}
void tr::renderer::Deferred::draw(Frame &frame, VkRect2D render_area, std::span<const DirectionalLight> lights) const {
  const DebugCmdScope scope(frame.cmd.vk_cmd, "Deferred");

  ImageMemoryBarrier::submit<4>(
      frame.cmd.vk_cmd,
      {{
          frame.frm.get_image(ImageRessourceId::Swapchain).invalidate().prepare_barrier(SyncColorAttachmentOutput),
          frame.frm.get_image(ImageRessourceId::GBuffer0).prepare_barrier(SyncFragmentStorageRead),
          frame.frm.get_image(ImageRessourceId::GBuffer1).prepare_barrier(SyncFragmentStorageRead),
          frame.frm.get_image(ImageRessourceId::GBuffer2).prepare_barrier(SyncFragmentStorageRead),
      }});
  std::array<VkRenderingAttachmentInfo, 1> attachments{
      frame.frm.get_image(ImageRessourceId::Swapchain).as_attachment(ImageClearOpDontCare{}),
  };

  const auto descriptor = frame.allocate_descriptor(descriptor_set_layouts[0]);
  DescriptorUpdater{descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
      .image_info({{
          {
              .sampler = VK_NULL_HANDLE,
              .imageView = frame.frm.get_image(ImageRessourceId::GBuffer0).view,
              .imageLayout = SyncFragmentStorageRead.layout,
          },
          {
              .sampler = VK_NULL_HANDLE,
              .imageView = frame.frm.get_image(ImageRessourceId::GBuffer1).view,
              .imageLayout = SyncFragmentStorageRead.layout,
          },
          {
              .sampler = VK_NULL_HANDLE,
              .imageView = frame.frm.get_image(ImageRessourceId::GBuffer2).view,
              .imageLayout = SyncFragmentStorageRead.layout,
          },
      }})
      .write(frame.ctx->device.vk_device);

  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor, 0,
                          nullptr);

  const VkRenderingInfo render_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderArea = render_area,
      .layerCount = 1,
      .viewMask = 0,
      .colorAttachmentCount = utils::narrow_cast<uint32_t>(attachments.size()),
      .pColorAttachments = attachments.data(),
      .pDepthAttachment = nullptr,
      .pStencilAttachment = nullptr,
  };
  vkCmdBeginRendering(frame.cmd.vk_cmd, &render_info);

  const VkViewport viewport{
      static_cast<float>(render_area.offset.x),
      static_cast<float>(render_area.offset.y),
      static_cast<float>(render_area.extent.width),
      static_cast<float>(render_area.extent.height),
      0.0,
      1.0,
  };
  vkCmdSetViewport(frame.cmd.vk_cmd, 0, 1, &viewport);
  vkCmdSetScissor(frame.cmd.vk_cmd, 0, 1, &render_area);
  vkCmdBindPipeline(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  //
  // TODO: use a vertex buffer
  for (const auto light : lights) {
    vkCmdPushConstants(frame.cmd.vk_cmd, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DirectionalLight),
                       &light);
    vkCmdDraw(frame.cmd.vk_cmd, 3, 1, 0, 0);
  }

  vkCmdEndRendering(frame.cmd.vk_cmd);
}
