#include "forward.h"

#include <bits/getopt_core.h>
#include <imgui.h>
#include <shaderc/env.h>
#include <shaderc/shaderc.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <filesystem>
#include <glm/fwd.hpp>
#include <memory>
#include <optional>
#include <shaderc/shaderc.hpp>
#include <string>

#include "../debug.h"
#include "../descriptors.h"
#include "../frame.h"
#include "../pipeline.h"
#include "../vulkan_engine.h"
#include "shadow_map.h"
#include "utils/misc.h"
struct PushConstant {
  tr::CameraInfo info;
  glm::vec3 color;
  float padding = 0.0;
};

void tr::renderer::Forward::init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm,
                                 Lifetime &setup_lifetime) {
  const std::array vert_spv_default = std::to_array<uint32_t>({
#include "shaders/forward.vert.inc"
  });

  const std::array frag_spv_default = std::to_array<uint32_t>({
#include "shaders/forward.frag.inc"
  });

  shadow_map_handle = rm.register_transient_image(SHADOW_MAP);
  rendered_handle = rm.register_transient_image(RENDERED);
  depth_handle = rm.register_transient_image(DEPTH);
  camera_handle = rm.register_transient_buffer(CAMERA);

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetIncluder(std::make_unique<FileIncluder>("./ToyRenderer/shaders"));
  options.SetGenerateDebugInfo();
  options.SetSourceLanguage(shaderc_source_language_glsl);
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
  if (pcf_enable) {
    options.AddMacroDefinition("PERCENTAGE_CLOSER_FILTERING");
    options.AddMacroDefinition("PERCENTAGE_CLOSER_FILTERING_ITER", std::format("{}", pcf_iter_count));
  }
  options.AddMacroDefinition("SHADOW_BIAS", std::format("{}", shadow_bias));

  const auto shader_stages = TIMED_INLINE_LAMBDA("Compiling forward shader") {
    const auto frag_spv =
        Shader::compile(compiler, shaderc_glsl_fragment_shader, options, "./ToyRenderer/shaders/forward.frag");
    const auto vert_spv =
        Shader::compile(compiler, shaderc_glsl_vertex_shader, options, "./ToyRenderer/shaders/forward.vert");

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

  const auto color_blend_attchment_states = std::to_array<VkPipelineColorBlendAttachmentState>({
      PipelineColorBlendStateAllColorBlend.build(),
  });

  const auto color_formats = std::to_array<VkFormat>({
      RENDERED.definition.vk_format(ctx.swapchain),
  });

  const auto color_blend_state = PipelineColorBlendStateBuilder{}.attachments(color_blend_attchment_states).build();
  auto pipeline_rendering_create_info = PipelineRenderingBuilder{}
                                            .color_attachment_formats(color_formats)
                                            .depth_attachment(DEPTH.definition.vk_format(ctx.swapchain))
                                            .build();

  descriptor_set_layouts = std::to_array({
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::Forward::set_0).build(ctx.device.vk_device),
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::Forward::set_1).build(ctx.device.vk_device),
  });
  const auto push_constant_ranges = std::to_array<VkPushConstantRange>({
      {
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = sizeof(glm::mat4x4),
      },
      {
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = sizeof(glm::mat4x4),
          .size = sizeof(PushConstant),
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
                 .color_blend_state(&color_blend_state)
                 .dynamic_state(&dynamic_state_state)
                 .build(ctx.device.vk_device);

  shadow_map_sampler = VK_NULL_HANDLE;
  const VkSamplerCreateInfo sampler_create_info{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      .mipLodBias = 0,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_NEVER,
      .minLod = 0,
      .maxLod = 0,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
  };
  VK_UNWRAP(vkCreateSampler, ctx.device.vk_device, &sampler_create_info, nullptr, &shadow_map_sampler);

  lifetime.tie(DeviceHandle::Pipeline, pipeline);
  lifetime.tie(DeviceHandle::PipelineLayout, pipeline_layout);
  lifetime.tie(DeviceHandle::Sampler, shadow_map_sampler);
  for (auto descriptor_set_layout : descriptor_set_layouts) {
    lifetime.tie(DeviceHandle::DescriptorSetLayout, descriptor_set_layout);
  }
}

void tr::renderer::Forward::draw_mesh(Frame &frame, const Frustum &frustum, const Mesh &mesh,
                                      const DefaultRessources &default_ressources) const {
  auto &shadow_map_ressource = frame.frm->get_image_ressource(shadow_map_handle);

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
    DescriptorUpdater{descriptor, 1}
        .type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        .image_info({{
            {
                .sampler = shadow_map_sampler,
                .imageView = shadow_map_ressource.view,
                .imageLayout = SyncFragmentStorageRead.layout,
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

void tr::renderer::Forward::end_draw(VkCommandBuffer cmd) const {
  utils::ignore_unused(this);
  vkCmdEndRendering(cmd);
}

void tr::renderer::Forward::start_draw(Frame &frame, VkRect2D render_area) const {
  const DebugCmdScope scope(frame.cmd.vk_cmd, "Forward");
  auto &rendered_ressource = frame.frm->get_image_ressource(rendered_handle);
  auto &depth_ressource = frame.frm->get_image_ressource(depth_handle);
  auto &shadow_map_ressource = frame.frm->get_image_ressource(shadow_map_handle);

  ImageMemoryBarrier::submit<3>(frame.cmd.vk_cmd, {{
                                                      rendered_ressource.prepare_barrier(SyncColorAttachmentOutput),
                                                      shadow_map_ressource.prepare_barrier(SyncFragmentStorageRead),
                                                      depth_ressource.prepare_barrier(SyncLateDepth),
                                                  }});
  const std::array<VkRenderingAttachmentInfo, 1> attachments{
      rendered_ressource.as_attachment(ImageClearOpLoad{}),
  };

  const VkRenderingAttachmentInfo depthAttachment = depth_ressource.as_attachment(ImageClearOpLoad{});

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

  const auto &b = frame.frm->get_buffer_ressource(camera_handle);
  const VkDescriptorBufferInfo buffer_info{b.buffer, 0, b.size};

  const auto camera_descriptor = frame.allocate_descriptor(descriptor_set_layouts[0]);
  DescriptorUpdater{camera_descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .buffer_info({&buffer_info, 1})
      .write(frame.ctx->ctx.device.vk_device);

  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &camera_descriptor,
                          0, nullptr);
}

auto tr::renderer::Forward::imgui() -> bool {
  bool rebuild = false;

  if (ImGui::CollapsingHeader("Forward", ImGuiTreeNodeFlags_DefaultOpen)) {
    rebuild |= ImGui::Button("Reload shader (Forward)");

    rebuild |= ImGui::Checkbox("Enable Percentage Closer filtering (Forward)", &pcf_enable);
    std::array const PCF_filter_sizes = utils::to_array<std::pair<const char *, uint8_t>>({
        {"0 (no filtering)", 0},
        {"1", 1},
        {"2", 2},
        {"3", 3},
        {"4", 4},
    });

    const auto current_pcf_filter_size = pcf_iter_count;
    if (pcf_enable &&
        ImGui::BeginCombo("PCF Filter Size (Forward)", std::format("{}", current_pcf_filter_size).c_str())) {
      for (const auto &size : PCF_filter_sizes) {
        if (ImGui::Selectable(size.first, current_pcf_filter_size == size.second)) {
          pcf_iter_count = size.second;
          rebuild = true;
        }
      }
      ImGui::EndCombo();
    }

    static utils::types::debouncer<bool> shader_bias_debouncer;
    shader_bias_debouncer.debounce(
        [&]() -> std::optional<bool> {
          if (ImGui::SliderFloat("Shadow bias (Forward)", &shadow_bias, 1e-15F, 0.01F, "%.5f",
                                 ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat)) {
            return true;
          }

          return std::nullopt;
        },
        [&](bool debounced) { rebuild |= debounced; });
  }
  return rebuild;
}
