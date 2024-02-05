#include "shadow_map.h"

#include <imgui.h>               // for BeginCombo, CollapsingHeader
#include <shaderc/env.h>         // for shaderc_env_version_vulkan_1_3
#include <shaderc/shaderc.h>     // for shaderc_glsl_vertex_shader, sha...
#include <utils/misc.h>          // for ignore_unused, TIMED_INLINE_LAMBDA
#include <vulkan/vulkan_core.h>  // for VkShaderStageFlagBits, VkDynami...

#include <algorithm>            // for copy, max
#include <array>                // for array, to_array
#include <format>               // for format
#include <glm/fwd.hpp>          // for mat4x4
#include <optional>             // for optional
#include <shaderc/shaderc.hpp>  // for CompileOptions, Compiler
#include <utility>              // for pair
#include <vector>               // for vector

#include "../../camera.h"             // for CameraInfo
#include "../../registry.h"           // for CVarExtent2D
#include "../buffer.h"                // for OneTimeCommandBuffer
#include "../context.h"               // for VulkanContext
#include "../debug.h"                 // for DebugCmdScope
#include "../deletion_stack.h"        // for DeviceHandle, Lifetime
#include "../descriptors.h"           // for DescriptorSetLayoutBuilder, Des...
#include "../device.h"                // for Device
#include "../frame.h"                 // for Frame
#include "../mesh.h"                  // for GeoSurface, Mesh, Vertex, Direc...
#include "../pipeline.h"              // for PipelineBuilder, PipelineLayout...
#include "../ressource_definition.h"  // for SHADOW_MAP, shadow_map_extent
#include "../ressource_manager.h"     // for FrameRessourceData, RessourceMa...
#include "../ressources.h"            // for BufferRessource, ImageRessource
#include "../synchronisation.h"       // for SyncLateDepth, ImageMemoryBarrier
#include "../vulkan_engine.h"         // for VulkanEngine
#include "utils/types.h"              // for not_null_pointer

void tr::renderer::ShadowMap::init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm,
                                   Lifetime &setup_lifetime) {
  const std::array vert_spv_default = std::to_array<uint32_t>({
#include "shaders/shadow_map.vert.inc"  // IWYU pragma: keep
  });

  rendered_handle = rm.register_transient_image(RENDERED);
  shadow_map_handle = rm.register_transient_image(SHADOW_MAP);
  shadow_camera_handle = rm.register_buffer(SHADOW_CAMERA);

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetGenerateDebugInfo();
  options.SetSourceLanguage(shaderc_source_language_glsl);
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);

  const auto shader_stages = TIMED_INLINE_LAMBDA("Compiling shadow map shader") {
    const auto vert_spv =
        Shader::compile(compiler, shaderc_glsl_vertex_shader, options, "./ToyRenderer/shaders/shadow_map.vert");

    const auto vert = Shader::init_from_spv(setup_lifetime, ctx.device.vk_device,
                                            vert_spv ? std::span{*vert_spv} : std::span{vert_spv_default});

    return std::to_array({
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

  auto pipeline_rendering_create_info =
      PipelineRenderingBuilder{}.depth_attachment(SHADOW_MAP.definition.vk_format(ctx.swapchain)).build();

  descriptor_set_layouts = std::to_array({
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::ShadowMap::set_0).build(ctx.device.vk_device),
  });
  const auto push_constant_ranges = std::to_array<VkPushConstantRange>({
      {
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = sizeof(glm::mat4x4),
      },
  });

  pipeline_layout = PipelineLayoutBuilder{}
                        .set_layouts(descriptor_set_layouts)
                        .push_constant_ranges(push_constant_ranges)
                        .build(ctx.device.vk_device);
  pipeline = PipelineBuilder{}
                 .stages(shader_stages)
                 .layout_(pipeline_layout)
                 .pipeline_rendering_create_info(&pipeline_rendering_create_info)
                 .vertex_input_state(&vertex_input_state)
                 .input_assembly_state(&input_assembly_state)
                 .viewport_state(&viewport_state)
                 .rasterization_state(&rasterizer_state)
                 .multisample_state(&multisampling_state)
                 .depth_stencil_state(&depth_state)
                 .dynamic_state(&dynamic_state_state)
                 .build(ctx.device.vk_device);

  lifetime.tie(DeviceHandle::Pipeline, pipeline);
  lifetime.tie(DeviceHandle::PipelineLayout, pipeline_layout);
  for (auto descriptor_set_layout : descriptor_set_layouts) {
    lifetime.tie(DeviceHandle::DescriptorSetLayout, descriptor_set_layout);
  }
}
void tr::renderer::ShadowMap::end_draw(VkCommandBuffer cmd) const {
  utils::ignore_unused(this);
  vkCmdEndRendering(cmd);
}

void tr::renderer::ShadowMap::start_draw(Frame &frame) const {
  auto &shadow_map = frame.frm->get_image_ressource(shadow_map_handle);
  ImageMemoryBarrier::submit<1>(frame.cmd.vk_cmd, {{
                                                      shadow_map.invalidate().prepare_barrier(SyncLateDepth),
                                                  }});

  const VkRenderingAttachmentInfo depthAttachment =
      shadow_map.as_attachment(VkClearValue{.depthStencil = {.depth = 1., .stencil = 0}});

  const VkRect2D render_area{{0, 0}, shadow_map.extent};

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
void tr::renderer::ShadowMap::draw(Frame &frame, const DirectionalLight &light, std::span<const Mesh> meshes) const {
  const DebugCmdScope scope(frame.cmd.vk_cmd, "Shadow map");

  start_draw(frame);

  frame.frm->update_buffer<CameraInfo>(frame.ctx->allocator, shadow_camera_handle,
                                       [&](CameraInfo *info) { *info = light.camera_info(); });

  const auto &b = frame.frm->get_buffer_ressource(shadow_camera_handle);
  const VkDescriptorBufferInfo buffer_info{b.buffer, 0, b.size};
  const auto camera_descriptor = frame.allocate_descriptor(descriptor_set_layouts[0]);
  DescriptorUpdater{camera_descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .buffer_info({&buffer_info, 1})
      .write(frame.ctx->ctx.device.vk_device);

  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &camera_descriptor,
                          0, nullptr);

  for (const auto &mesh : meshes) {
    draw_mesh(frame, mesh);
  }
  end_draw(frame.cmd.vk_cmd);
}

void tr::renderer::ShadowMap::imgui(RessourceManager &rm) const {
  utils::ignore_unused(this);

  if (ImGui::CollapsingHeader("ShadowMap", ImGuiTreeNodeFlags_DefaultOpen)) {
    std::array const shadow_map_sizes = utils::to_array<std::pair<const char *, uint32_t>>({
        {"512x512", 512},
        {"1024x1024", 1024},
        {"2048x2048", 2048},
        {"4096x4096", 4096},
    });

    const auto current_shadow_map_size = shadow_map_extent.resolve();

    if (ImGui::BeginCombo(
            "Shadow Map Size",
            std::format("{}x{}", current_shadow_map_size.width, current_shadow_map_size.height).c_str())) {
      for (const auto &size : shadow_map_sizes) {
        if (ImGui::Selectable(size.first, current_shadow_map_size.width == size.second)) {
          shadow_map_extent.save({size.second, size.second});
          // TODO: does not work
          /* rm.clear_pool_if([](const ImageDefinition &def) { return def.depends_on(ImageDependency::CVAR); }, */
          /*                  [](const auto &) { */
          /*                    // TODO: Lifetime */
          /*                  }); */
        }
      }
      ImGui::EndCombo();
    }
  }
}
