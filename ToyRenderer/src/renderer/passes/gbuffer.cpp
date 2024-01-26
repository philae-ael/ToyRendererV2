#include "gbuffer.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <glm/mat4x4.hpp>

#include "../mesh.h"
#include "../pipeline.h"
#include "../vulkan_engine.h"
#include "frustrum_culling.h"
#include "utils/misc.h"

const std::array vert_spv_default = std::to_array<uint32_t>({
#include "shaders/gbuffer.vert.inc"
});

const std::array frag_spv_default = std::to_array<uint32_t>({
#include "shaders/gbuffer.frag.inc"
});

auto tr::renderer::GBuffer::init(Lifetime &lifetime, VulkanContext &ctx, const RessourceManager &rm,
                                 Lifetime &setup_lifetime) -> GBuffer {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetGenerateDebugInfo();
  options.SetSourceLanguage(shaderc_source_language_glsl);
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);

  const auto shader_stages = TIMED_INLINE_LAMBDA("Compiling deferred shader") {
    const auto frag_spv =
        Shader::compile(compiler, shaderc_glsl_fragment_shader, options, "./ToyRenderer/shaders/gbuffer.frag");
    const auto vert_spv =
        Shader::compile(compiler, shaderc_glsl_vertex_shader, options, "./ToyRenderer/shaders/gbuffer.vert");

    const auto frag = Shader::init_from_spv(setup_lifetime, ctx.device.vk_device,
                                            frag_spv ? std::span{*frag_spv} : std::span{frag_spv_default});
    const auto vert = Shader::init_from_spv(setup_lifetime, ctx.device.vk_device,
                                            vert_spv ? std::span{*vert_spv} : std::span{vert_spv_default});

    return std::to_array({
        frag.pipeline_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, "main"),
        vert.pipeline_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, "main"),
    });
  };

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

  const std::array color_blend_attchment_states = std::to_array<VkPipelineColorBlendAttachmentState, 4>({
      PipelineColorBlendStateAllColorNoBlend.build(),
      PipelineColorBlendStateAllColorNoBlend.build(),
      PipelineColorBlendStateAllColorNoBlend.build(),
      PipelineColorBlendStateAllColorNoBlend.build(),
  });

  const std::array<VkFormat, 4> color_formats{
      rm.image_definition(ImageRessourceId::GBuffer0).vk_format(ctx.swapchain),
      rm.image_definition(ImageRessourceId::GBuffer1).vk_format(ctx.swapchain),
      rm.image_definition(ImageRessourceId::GBuffer2).vk_format(ctx.swapchain),
      rm.image_definition(ImageRessourceId::GBuffer3).vk_format(ctx.swapchain),
  };

  const auto color_blend_state = PipelineColorBlendStateBuilder{}.attachments(color_blend_attchment_states).build();
  auto pipeline_rendering_create_info =
      PipelineRenderingBuilder{}
          .color_attachment_formats(color_formats)
          .depth_attachment(rm.image_definition(ImageRessourceId::Depth).vk_format(ctx.swapchain))
          .build();

  const auto descriptor_set_layouts = std::to_array({
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::GBuffer::set_0).build(ctx.device.vk_device),
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::GBuffer::set_1).build(ctx.device.vk_device),
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
                          .build(ctx.device.vk_device);
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
                            .build(ctx.device.vk_device);

  lifetime.tie(DeviceHandle::Pipeline, pipeline);
  lifetime.tie(DeviceHandle::PipelineLayout, layout);
  for (auto descriptor_set_layout : descriptor_set_layouts) {
    lifetime.tie(DeviceHandle::DescriptorSetLayout, descriptor_set_layout);
  }

  return {descriptor_set_layouts, layout, pipeline};
}

void tr::renderer::GBuffer::end_draw(VkCommandBuffer cmd) const {
  utils::ignore_unused(this);
  vkCmdEndRendering(cmd);
}

void tr::renderer::GBuffer::start_draw(Frame &frame, VkRect2D render_area) const {
  ImageMemoryBarrier::submit<5>(
      frame.cmd.vk_cmd,
      {{
          frame.frm.get_image(ImageRessourceId::GBuffer0).invalidate().prepare_barrier(SyncColorAttachmentOutput),
          frame.frm.get_image(ImageRessourceId::GBuffer1).invalidate().prepare_barrier(SyncColorAttachmentOutput),
          frame.frm.get_image(ImageRessourceId::GBuffer2).invalidate().prepare_barrier(SyncColorAttachmentOutput),
          frame.frm.get_image(ImageRessourceId::GBuffer3).invalidate().prepare_barrier(SyncColorAttachmentOutput),
          frame.frm.get_image(ImageRessourceId::Depth).invalidate().prepare_barrier(SyncLateDepth),
      }});

  const std::array attachments = utils::to_array<VkRenderingAttachmentInfo>({
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
      .write(frame.ctx->ctx.device.vk_device);

  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &camera_descriptor,
                          0, nullptr);
}

void tr::renderer::GBuffer::draw_mesh(Frame &frame, const Frustum &frustum, const Mesh &mesh,
                                      const DefaultRessources &default_ressources) const {
  const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(frame.cmd.vk_cmd, 0, 1, &mesh.buffers.vertices.buffer, &offset);
  if (mesh.buffers.indices) {
    vkCmdBindIndexBuffer(frame.cmd.vk_cmd, mesh.buffers.indices->buffer, 0, VK_INDEX_TYPE_UINT32);
  }

  vkCmdPushConstants(frame.cmd.vk_cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4x4),
                     &mesh.transform);

  std::span<const GeoSurface> const surfaces = mesh.surfaces;
  for (const auto &surface : FrustrumCulling::filter(frustum, surfaces)) {
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
        .write(frame.ctx->ctx.device.vk_device);
    vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 1, 1, &descriptor, 0,
                            nullptr);

    if (mesh.buffers.indices) {
      vkCmdDrawIndexed(frame.cmd.vk_cmd, surface.count, 1, surface.start, 0, 0);
    } else {
      vkCmdDraw(frame.cmd.vk_cmd, surface.count, 1, surface.start, 0);
    }
  }
}
