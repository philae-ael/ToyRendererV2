#include "gbuffer.h"

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <glm/mat4x4.hpp>

#include "../mesh.h"
#include "../pipeline.h"
#include "../render_graph.h"
#include "../vulkan_engine.h"
#include "utils/misc.h"

const std::array gbuffer_vert_bin = std::to_array<uint32_t>({
#include "shaders/gbuffer.vert.inc"
});

const std::array gbuffer_frag_bin = std::to_array<uint32_t>({
#include "shaders/gbuffer.frag.inc"
});

auto tr::renderer::GBuffer::init(VkDevice &device, Swapchain &swapchain, DeviceDeletionStack &device_deletion_stack)
    -> GBuffer {
  const auto frag = Shader::init_from_src(device, gbuffer_frag_bin);
  const auto vert = Shader::init_from_src(device, gbuffer_vert_bin);
  frag.defer_deletion(device_deletion_stack);
  vert.defer_deletion(device_deletion_stack);

  const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{{
      frag.pipeline_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, "main"),
      vert.pipeline_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, "main"),
  }};

  const std::array<VkDynamicState, 2> dynamic_states = {
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VIEWPORT,
  };
  const auto dynamic_state_state = PipelineDynamicStateBuilder{}.dynamic_state(dynamic_states).build();

  const auto vertex_input_state =
      PipelineVertexInputStateBuilder{}.vertex_attributes(Vertex::attributes).vertex_bindings(Vertex::bindings).build();

  const auto input_assembly_state =
      PipelineInputAssemblyBuilder{}.topology_(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST).build();
  const auto viewport_state = PipelineViewportStateBuilder{}.viewports_count(1).scissors_count(1).build();
  const auto rasterizer_state = PipelineRasterizationStateBuilder{}.cull_mode(VK_CULL_MODE_BACK_BIT).build();
  const auto multisampling_state = PipelineMultisampleStateBuilder{}.build();
  const auto depth_state = DepthStateTestAndWriteOpLess.build();

  const std::array color_blend_attchment_states =
      std::to_array<VkPipelineColorBlendAttachmentState, attachments_color.size()>({
          PipelineColorBlendStateAllColorNoBlend.build(),
          PipelineColorBlendStateAllColorNoBlend.build(),
          PipelineColorBlendStateAllColorNoBlend.build(),
          PipelineColorBlendStateAllColorNoBlend.build(),
      });

  const std::array<VkFormat, attachments_color.size()> color_formats{
      attachments_color[0].definition.vk_format(swapchain),
      attachments_color[1].definition.vk_format(swapchain),
      attachments_color[2].definition.vk_format(swapchain),
      attachments_color[3].definition.vk_format(swapchain),
  };

  const auto color_blend_state = PipelineColorBlendStateBuilder{}.attachments(color_blend_attchment_states).build();
  auto pipeline_rendering_create_info = PipelineRenderingBuilder{}
                                            .color_attachment_formats(color_formats)
                                            .depth_attachment(attachment_depth.definition.vk_format(swapchain))
                                            .build();

  const auto descriptor_set_layouts = std::to_array({
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::GBuffer::set_0).build(device),
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::GBuffer::set_1).build(device),
  });
  const auto push_constant_ranges = std::to_array<VkPushConstantRange>({
      {
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = sizeof(glm::mat4x4),
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
void tr::renderer::GBuffer::end_draw(VkCommandBuffer cmd) const {
  utils::ignore_unused(this);
  vkCmdEndRendering(cmd);
}

void tr::renderer::GBuffer::start_draw(Frame &frame, VkRect2D render_area) const {
  ImageMemoryBarrier::submit<attachments_color.size() + 1>(
      frame.cmd.vk_cmd,
      {{
          frame.frm.get_image(ImageRessourceId::GBuffer0).invalidate().prepare_barrier(SyncColorAttachmentOutput),
          frame.frm.get_image(ImageRessourceId::GBuffer1).invalidate().prepare_barrier(SyncColorAttachmentOutput),
          frame.frm.get_image(ImageRessourceId::GBuffer2).invalidate().prepare_barrier(SyncColorAttachmentOutput),
          frame.frm.get_image(ImageRessourceId::GBuffer3).invalidate().prepare_barrier(SyncColorAttachmentOutput),
          frame.frm.get_image(ImageRessourceId::Depth).invalidate().prepare_barrier(SyncLateDepth),
      }});

  const std::array attachments = utils::to_array<VkRenderingAttachmentInfo, attachments_color.size()>({
      frame.frm.get_image(ImageRessourceId::GBuffer0)
          .as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
      frame.frm.get_image(ImageRessourceId::GBuffer1)
          .as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
      frame.frm.get_image(ImageRessourceId::GBuffer2)
          .as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
      frame.frm.get_image(ImageRessourceId::GBuffer3)
          .as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
  });

  const VkRenderingAttachmentInfo depthAttachment =
      frame.frm.get_image(ImageRessourceId::Depth)
          .as_attachment(VkClearValue{.depthStencil = {.depth = 1., .stencil = 0}});

  const VkRenderingInfo render_info{
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

  const auto &b = frame.frm.get_buffer(BufferRessourceId::Camera);
  const VkDescriptorBufferInfo buffer_info{b.buffer, 0, b.size};

  const auto camera_descriptor = frame.allocate_descriptor(descriptor_set_layouts[0]);
  DescriptorUpdater{camera_descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .buffer_info({&buffer_info, 1})
      .write(frame.ctx->device.vk_device);

  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &camera_descriptor,
                          0, nullptr);
}

void tr::renderer::GBuffer::draw_mesh(Frame &frame, const Mesh &mesh, const DefaultRessources &default_ressources) {
  const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(frame.cmd.vk_cmd, 0, 1, &mesh.buffers.vertices.buffer, &offset);
  if (mesh.buffers.indices) {
    vkCmdBindIndexBuffer(frame.cmd.vk_cmd, mesh.buffers.indices->buffer, 0, VK_INDEX_TYPE_UINT32);
  }

  vkCmdPushConstants(frame.cmd.vk_cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4x4),
                     &mesh.transform);

  for (const auto &surface : mesh.surfaces) {
    auto descriptor = frame.allocate_descriptor(descriptor_set_layouts[1]);

    DescriptorUpdater{descriptor, 0}
        .type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        .image_info({{
            {
                .sampler = default_ressources.sampler,
                .imageView = surface.material->base_color_texture.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            {
                .sampler = default_ressources.sampler,
                .imageView =
                    surface.material->metallic_roughness_texture.value_or(default_ressources.metallic_roughness).view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            {
                .sampler = default_ressources.sampler,
                .imageView = surface.material->normal_texture.value_or(default_ressources.normal_map).view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
        }})
        .write(frame.ctx->device.vk_device);
    vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 1, 1, &descriptor, 0,
                            nullptr);

    if (mesh.buffers.indices) {
      vkCmdDrawIndexed(frame.cmd.vk_cmd, surface.count, 1, surface.start, 0, 0);
    } else {
      vkCmdDraw(frame.cmd.vk_cmd, surface.count, 1, surface.start, 0);
    }
  }
}
void tr::renderer::GBuffer::draw(Frame &frame, VkRect2D render_area, std::span<const Mesh> meshes,
                                 DefaultRessources default_ressources) {
  const DebugCmdScope scope(frame.cmd.vk_cmd, "GBuffer");

  start_draw(frame, render_area);
  for (const auto &mesh : meshes) {
    draw_mesh(frame, mesh, default_ressources);
  }
  end_draw(frame.cmd.vk_cmd);
}
