#include "gbuffer.h"

#include <shaderc/env.h>      // for shaderc_env_version_vulkan_1_3
#include <shaderc/shaderc.h>  // for shaderc_glsl_fragment_shader
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>  // for VkShaderStageFlagBits, VkDescri...

#include <array>                // for array, to_array
#include <cstdint>              // for uint32_t
#include <glm/fwd.hpp>          // for mat4x4
#include <optional>             // for optional
#include <shaderc/shaderc.hpp>  // for CompileOptions, Compiler
#include <span>                 // for span
#include <vector>               // for vector, allocator

#include "../context.h"               // for VulkanContext
#include "../descriptors.h"           // for DescriptorSetLayoutBindingBuilder
#include "../device.h"                // for Device
#include "../frame.h"                 // for Frame
#include "../mesh.h"                  // for GeoSurface, MaterialHandles, Mesh
#include "../pipeline.h"              // for PipelineColorBlendAttachmentSta...
#include "../ressource_definition.h"  // for GBUFFER_0, GBUFFER_1, GBUFFER_2
#include "../ressource_manager.h"     // for FrameRessourceData
#include "../ressources.h"            // for BufferRessourceDefinition, Imag...
#include "../synchronisation.h"       // for SyncColorAttachmentOutput, Sync...
#include "../vulkan_engine.h"         // for VulkanEngine
#include "frustrum_culling.h"         // for FrustrumCulling
#include "pass.h"                     // for ColorAttachment, PassInfo, Basi...
#include "utils/cast.h"               // for narrow_cast, to_array
#include "utils/misc.h"               // for ignore_unused
#include "utils/types.h"              // for not_null_pointer

namespace tr::renderer {
constexpr std::array gbuffer_vert_spv = std::to_array<uint32_t>({
#include "shaders/gbuffer.vert.inc"  // IWYU pragma: keep
});

constexpr std::array gbuffer_frag_spv = std::to_array<uint32_t>({
#include "shaders/gbuffer.frag.inc"  // IWYU pragma: keep
});

const PassDefinition gbuffer_pass{
    .shaders =
        {
            ShaderDefininition{
                .kind = shaderc_glsl_fragment_shader,
                .entry_point = "main",
                .runtime_path = "./ToyRenderer/shaders/gbuffer.frag",
                .compile_time_spv = {gbuffer_frag_spv.begin(), gbuffer_frag_spv.end()},
            },
            ShaderDefininition{
                .kind = shaderc_glsl_vertex_shader,
                .entry_point = "main",
                .runtime_path = "./ToyRenderer/shaders/gbuffer.vert",
                .compile_time_spv = {gbuffer_vert_spv.begin(), gbuffer_vert_spv.end()},
            },
        },
    .descriptor_sets =
        {
            {
                DescriptorSetLayoutBindingBuilder{}
                    .binding_(0)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    .descriptor_count(1)
                    .stages(VK_SHADER_STAGE_VERTEX_BIT)
                    .build(),
            },
            {
                DescriptorSetLayoutBindingBuilder{}
                    .binding_(0)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_SAMPLER)
                    .descriptor_count(1)
                    .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build(),
                DescriptorSetLayoutBindingBuilder{}
                    .binding_(1)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                    .descriptor_count(1024)
                    .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build(),
            },
        },
    .push_constants =
        {
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = sizeof(glm::mat4x4),
            },
            {
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = sizeof(glm::mat4x4),
                .size = 3 * sizeof(uint32_t),
            },
        },
    .inputs =
        {
            .images = {},
            .buffers = {CAMERA},
        },
    .outputs =
        {
            .color_attachments =
                {
                    ColorAttachment{GBUFFER_0, PipelineColorBlendStateAllColorNoBlend.build()},
                    ColorAttachment{GBUFFER_1, PipelineColorBlendStateAllColorNoBlend.build()},
                    ColorAttachment{GBUFFER_2, PipelineColorBlendStateAllColorNoBlend.build()},
                    ColorAttachment{GBUFFER_3, PipelineColorBlendStateAllColorNoBlend.build()},
                },
            .depth_attachement = DEPTH,
            .buffers = {},
        },
};

constexpr BasicPipelineDefinition gbuffer_pipeline{
    .vertex_input_state = PipelineVertexInputStateBuilder{}
                              .vertex_attributes(Vertex::attributes)
                              .vertex_bindings(Vertex::bindings)
                              .build(),
    .input_assembly_state = PipelineInputAssemblyBuilder{}.topology_(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST).build(),
    .rasterizer_state = PipelineRasterizationStateBuilder{}.cull_mode(VK_CULL_MODE_BACK_BIT).build(),
    .depth_state = DepthStateTestAndWriteOpLess.build(),
};

void GBuffer::init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetGenerateDebugInfo();
  options.SetSourceLanguage(shaderc_source_language_glsl);
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
  pass_info = gbuffer_pass.build(lifetime, ctx, rm, setup_lifetime, compiler, options);
  pipeline = gbuffer_pipeline.build(lifetime, ctx, pass_info);
}

void GBuffer::end_draw(VkCommandBuffer cmd) const {
  utils::ignore_unused(this);
  vkCmdEndRendering(cmd);
}

void GBuffer::start_draw(Frame &frame, VkRect2D render_area, const DefaultRessources &default_ressources) const {
  std::array<utils::types::not_null_pointer<ImageRessource>, 4> gbuffer_ressource{
      frame.frm->get_image_ressource(pass_info.outputs.color_attachments[0]),
      frame.frm->get_image_ressource(pass_info.outputs.color_attachments[1]),
      frame.frm->get_image_ressource(pass_info.outputs.color_attachments[2]),
      frame.frm->get_image_ressource(pass_info.outputs.color_attachments[3]),
  };
  ImageRessource &depth_ressource{frame.frm->get_image_ressource(pass_info.outputs.depth_attachement)};

  ImageMemoryBarrier::submit<5>(frame.cmd.vk_cmd,
                                {{
                                    gbuffer_ressource[0]->invalidate().prepare_barrier(SyncColorAttachmentOutput),
                                    gbuffer_ressource[1]->invalidate().prepare_barrier(SyncColorAttachmentOutput),
                                    gbuffer_ressource[2]->invalidate().prepare_barrier(SyncColorAttachmentOutput),
                                    gbuffer_ressource[3]->invalidate().prepare_barrier(SyncColorAttachmentOutput),
                                    depth_ressource.invalidate().prepare_barrier(SyncLateDepth),
                                }});

  const std::array attachments = utils::to_array<VkRenderingAttachmentInfo>({
      gbuffer_ressource[0]->as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
      gbuffer_ressource[1]->as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
      gbuffer_ressource[2]->as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
      gbuffer_ressource[3]->as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 0.0, 0.0}}}),
  });

  const VkRenderingAttachmentInfo depthAttachment =
      depth_ressource.as_attachment(VkClearValue{.depthStencil = {.depth = 1., .stencil = 0}});

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

  const auto &b = frame.frm->get_buffer_ressource(pass_info.inputs.buffers[0]);
  const VkDescriptorBufferInfo buffer_info{b.buffer, 0, b.size};

  const auto camera_descriptor = frame.allocate_descriptor(pass_info.descriptor_set_layouts[0]);
  DescriptorUpdater{camera_descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .buffer_info({&buffer_info, 1})
      .write(frame.ctx->ctx.device.vk_device);

  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_info.pipeline_layout, 0, 1,
                          &camera_descriptor, 0, nullptr);

  const auto tex_descriptor = frame.allocate_descriptor(pass_info.descriptor_set_layouts[1]);
  DescriptorUpdater{tex_descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_SAMPLER)
      .image_info({{
          VkDescriptorImageInfo{
              .sampler = default_ressources.sampler,
              .imageView = VK_NULL_HANDLE,
              .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          },
      }})
      .write(frame.ctx->ctx.device.vk_device);
  DescriptorUpdater{tex_descriptor, 1}
      .type(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
      .image_info(std::span{frame.frm->descriptor_image_infos}.subspan(0, frame.frm->external_images_offset))
      .write(frame.ctx->ctx.device.vk_device);

  std::array descrs{camera_descriptor, tex_descriptor};
  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_info.pipeline_layout, 0,
                          descrs.size(), descrs.data(), 0, nullptr);
}

void GBuffer::draw_mesh(Frame &frame, const Frustum &frustum, const Mesh &mesh,
                        const DefaultRessources &default_ressources) const {
  const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(frame.cmd.vk_cmd, 0, 1, &mesh.buffers.vertices.buffer, &offset);
  if (mesh.buffers.indices) {
    vkCmdBindIndexBuffer(frame.cmd.vk_cmd, mesh.buffers.indices->buffer, 0, VK_INDEX_TYPE_UINT32);
  }

  vkCmdPushConstants(frame.cmd.vk_cmd, pass_info.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4x4),
                     &mesh.transform);

  int i = 0;
  std::span<const GeoSurface> const surfaces = mesh.surfaces;
  for (const auto &surface : FrustrumCulling::filter(frustum, surfaces)) {
    i++;
    const struct {
      uint32_t albedo_idx;
      uint32_t normal_idx;
      uint32_t metallic_roughness_idx;
    } idx{
        .albedo_idx = frame.frm->image_index(surface.material.albedo_handle),
        .normal_idx =
            frame.frm->image_index(surface.material.normal_handle.value_or(default_ressources.normal_map_handle)),
        .metallic_roughness_idx = frame.frm->image_index(
            surface.material.metallic_roughness_handle.value_or(default_ressources.metallic_roughness_handle)),
    };
    vkCmdPushConstants(frame.cmd.vk_cmd, pass_info.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4x4),
                       sizeof(idx), &idx);

    if (mesh.buffers.indices) {
      vkCmdDrawIndexed(frame.cmd.vk_cmd, surface.count, 1, surface.start, 0, 0);
    } else {
      vkCmdDraw(frame.cmd.vk_cmd, surface.count, 1, surface.start, 0);
    }
  }

  spdlog::trace("surface drawn: {}", i);
}
}  // namespace tr::renderer
