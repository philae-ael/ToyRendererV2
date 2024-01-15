#include "shadow_map.h"

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <glm/mat4x4.hpp>

#include "../mesh.h"
#include "../pipeline.h"
#include "../ressource_definition.h"
#include "../vulkan_engine.h"
#include "utils/misc.h"

const std::array gbuffer_vert_bin = std::to_array<uint32_t>({
#include "shaders/shadow_map.vert.inc"
});

auto tr::renderer::ShadowMap::init(VkDevice &device, const RessourceManager &rm, const Swapchain &swapchain,
                                   DeviceDeletionStack &device_deletion_stack) -> ShadowMap {
  const auto vert = Shader::init_from_src(device, gbuffer_vert_bin);
  vert.defer_deletion(device_deletion_stack);

  const std::array shader_stages = std::to_array<VkPipelineShaderStageCreateInfo>({
      vert.pipeline_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, "main"),
  });

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

  auto pipeline_rendering_create_info =
      PipelineRenderingBuilder{}
          .depth_attachment(rm.image_definition(ImageRessourceId::ShadowMap).vk_format(swapchain))
          .build();

  const auto descriptor_set_layouts = std::to_array({
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::ShadowMap::set_0).build(device),
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
                            .dynamic_state(&dynamic_state_state)
                            .build(device);

  return {descriptor_set_layouts, layout, pipeline};
}
void tr::renderer::ShadowMap::end_draw(VkCommandBuffer cmd) const {
  utils::ignore_unused(this);
  vkCmdEndRendering(cmd);
}

void tr::renderer::ShadowMap::start_draw(Frame &frame) const {
  ImageMemoryBarrier::submit<1>(
      frame.cmd.vk_cmd,
      {{
          frame.frm.get_image(ImageRessourceId::ShadowMap).invalidate().prepare_barrier(SyncLateDepth),
      }});

  const VkRenderingAttachmentInfo depthAttachment =
      frame.frm.get_image(ImageRessourceId::ShadowMap)
          .as_attachment(VkClearValue{.depthStencil = {.depth = 1., .stencil = 0}});

  const VkRect2D render_area{{0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}};

  const VkRenderingInfo render_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderArea = render_area,
      .layerCount = 1,
      .viewMask = 0,
      .colorAttachmentCount = 0,
      .pColorAttachments = nullptr,
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

void tr::renderer::ShadowMap::draw_mesh(Frame &frame, const Mesh &mesh) const {
  const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(frame.cmd.vk_cmd, 0, 1, &mesh.buffers.vertices.buffer, &offset);
  if (mesh.buffers.indices) {
    vkCmdBindIndexBuffer(frame.cmd.vk_cmd, mesh.buffers.indices->buffer, 0, VK_INDEX_TYPE_UINT32);
  }

  vkCmdPushConstants(frame.cmd.vk_cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4x4),
                     &mesh.transform);

  for (const auto &surface : mesh.surfaces) {
    if (mesh.buffers.indices) {
      vkCmdDrawIndexed(frame.cmd.vk_cmd, surface.count, 1, surface.start, 0, 0);
    } else {
      vkCmdDraw(frame.cmd.vk_cmd, surface.count, 1, surface.start, 0);
    }
  }
}
void tr::renderer::ShadowMap::draw(Frame &frame, const DirectionalLight &light, std::span<const Mesh> meshes) {
  const DebugCmdScope scope(frame.cmd.vk_cmd, "Shadow map");

  start_draw(frame);

  frame.frm.update_buffer<CameraInfo>(frame.ctx->allocator, BufferRessourceId::ShadowCamera,
                                      [&](CameraInfo *info) { *info = light.camera_info(); });

  const auto &b = frame.frm.get_buffer(BufferRessourceId::ShadowCamera);
  const VkDescriptorBufferInfo buffer_info{b.buffer, 0, b.size};
  const auto camera_descriptor = frame.allocate_descriptor(descriptor_set_layouts[0]);
  DescriptorUpdater{camera_descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .buffer_info({&buffer_info, 1})
      .write(frame.ctx->device.vk_device);

  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &camera_descriptor,
                          0, nullptr);

  for (const auto &mesh : meshes) {
    draw_mesh(frame, mesh);
  }
  end_draw(frame.cmd.vk_cmd);
}