#include "deferred.h"

#include <bits/getopt_core.h>
#include <imgui.h>
#include <shaderc/env.h>
#include <shaderc/shaderc.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <utils/misc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <glm/fwd.hpp>
#include <memory>
#include <optional>
#include <shaderc/shaderc.hpp>
#include <string>

#include "../debug.h"
#include "../descriptors.h"
#include "../frame.h"
#include "../mesh.h"
#include "../pipeline.h"
#include "../ressource_definition.h"
#include "../vulkan_engine.h"
#include "pass.h"
#include "shadow_map.h"

struct PushConstant {
  tr::CameraInfo info;
  glm::vec3 color;
  float padding = 0.0;
};

namespace tr::renderer {

constexpr std::array deferred_frag_spv = std::to_array<uint32_t>({
#include "shaders/deferred.frag.inc"
});
constexpr std::array deferred_vert_spv = std::to_array<uint32_t>({
#include "shaders/deferred.vert.inc"
});

const PassDefinition deferred_pass{
    .shaders =
        {
            ShaderDefininition{
                .kind = shaderc_glsl_fragment_shader,
                .entry_point = "main",
                .runtime_path = "./ToyRenderer/shaders/deferred.frag",
                .compile_time_spv = {deferred_frag_spv.begin(), deferred_frag_spv.end()},
            },
            ShaderDefininition{
                .kind = shaderc_glsl_vertex_shader,
                .entry_point = "main",
                .runtime_path = "./ToyRenderer/shaders/deferred.vert",
                .compile_time_spv = {deferred_vert_spv.begin(), deferred_vert_spv.end()},
            },
        },
    .descriptor_sets =
        {
            {
                DescriptorSetLayoutBindingBuilder{}
                    .binding_(0)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .descriptor_count(4)
                    .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build(),
                DescriptorSetLayoutBindingBuilder{}
                    .binding_(1)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .descriptor_count(1)
                    .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build(),
            },
        },
    .push_constants =
        {
            {
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = sizeof(PushConstant),
            },
        },
    .inputs =
        {
            .images = {GBUFFER_0, GBUFFER_1, GBUFFER_2, GBUFFER_3, SHADOW_MAP},
            .buffers = {CAMERA},
        },
    .outputs =
        {
            .color_attachments = {ColorAttachment{RENDERED, PipelineColorBlendStateAllColorNoBlend.build()}},
            .depth_attachement = {},
            .buffers = {},
        },
};

constexpr BasicPipelineDefinition deferred_pipeline{
    .vertex_input_state = PipelineVertexInputStateBuilder{}.build(),
    .input_assembly_state = PipelineInputAssemblyBuilder{}.topology_(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST).build(),
    .rasterizer_state = PipelineRasterizationStateBuilder{}.build(),
    .depth_state = DepthStateTestAndWriteOpLess.build(),
};

void Deferred::init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime) {
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

  pass_info = deferred_pass.build(lifetime, ctx, rm, setup_lifetime, compiler, options);
  pipeline = deferred_pipeline.build(lifetime, ctx, pass_info);

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
  lifetime.tie(DeviceHandle::Sampler, shadow_map_sampler);
}

void Deferred::draw(Frame &frame, VkRect2D render_area, std::span<const DirectionalLight> lights) const {
  const DebugCmdScope scope(frame.cmd.vk_cmd, "Deferred");

  std::array<utils::types::not_null_pointer<ImageRessource>, 4> gbuffer_ressource{
      frame.frm->get_image_ressource(pass_info.inputs.images[0]),
      frame.frm->get_image_ressource(pass_info.inputs.images[1]),
      frame.frm->get_image_ressource(pass_info.inputs.images[2]),
      frame.frm->get_image_ressource(pass_info.inputs.images[3]),
  };
  ImageRessource &shadow_map_ressource{frame.frm->get_image_ressource(pass_info.inputs.images[4])};
  ImageRessource &rendered_ressource{frame.frm->get_image_ressource(pass_info.outputs.color_attachments[0])};

  ImageMemoryBarrier::submit<6>(frame.cmd.vk_cmd,
                                {{
                                    rendered_ressource.invalidate().prepare_barrier(SyncColorAttachmentOutput),
                                    gbuffer_ressource[0]->prepare_barrier(SyncFragmentStorageRead),
                                    gbuffer_ressource[1]->prepare_barrier(SyncFragmentStorageRead),
                                    gbuffer_ressource[2]->prepare_barrier(SyncFragmentStorageRead),
                                    gbuffer_ressource[3]->prepare_barrier(SyncFragmentStorageRead),
                                    shadow_map_ressource.prepare_barrier(SyncFragmentStorageRead),
                                }});
  std::array<VkRenderingAttachmentInfo, 1> attachments{
      rendered_ressource.as_attachment(ImageClearOpDontCare{}),
  };

  const auto descriptor = frame.allocate_descriptor(pass_info.descriptor_set_layouts[0]);
  DescriptorUpdater{descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
      .image_info({{
          {
              .sampler = VK_NULL_HANDLE,
              .imageView = gbuffer_ressource[0]->view,
              .imageLayout = SyncFragmentStorageRead.layout,
          },
          {
              .sampler = VK_NULL_HANDLE,
              .imageView = gbuffer_ressource[1]->view,
              .imageLayout = SyncFragmentStorageRead.layout,
          },
          {
              .sampler = VK_NULL_HANDLE,
              .imageView = gbuffer_ressource[2]->view,
              .imageLayout = SyncFragmentStorageRead.layout,
          },
          {
              .sampler = VK_NULL_HANDLE,
              .imageView = gbuffer_ressource[3]->view,
              .imageLayout = SyncFragmentStorageRead.layout,
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

  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass_info.pipeline_layout, 0, 1,
                          &descriptor, 0, nullptr);

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

  // TODO: use a vertex buffer
  for (const auto light : lights) {
    PushConstant data{
        light.camera_info(),
        light.color,
    };

    vkCmdPushConstants(frame.cmd.vk_cmd, pass_info.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(data),
                       &data);
    vkCmdDraw(frame.cmd.vk_cmd, 3, 1, 0, 0);
  }

  vkCmdEndRendering(frame.cmd.vk_cmd);
}

auto Deferred::imgui() -> bool {
  bool rebuild = false;

  if (ImGui::CollapsingHeader("Deferred", ImGuiTreeNodeFlags_DefaultOpen)) {
    rebuild |= ImGui::Button("Reload shader");

    rebuild |= ImGui::Checkbox("Enable Percentage Closer filtering", &pcf_enable);
    std::array const PCF_filter_sizes = utils::to_array<std::pair<const char *, uint8_t>>({
        {"0 (no filtering)", 0},
        {"1", 1},
        {"2", 2},
        {"3", 3},
        {"4", 4},
    });

    const auto current_pcf_filter_size = pcf_iter_count;
    if (pcf_enable && ImGui::BeginCombo("PCF Filter Size", std::format("{}", current_pcf_filter_size).c_str())) {
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
          if (ImGui::SliderFloat("Shadow bias", &shadow_bias, 1e-15F, 0.01F, "%.5f",
                                 ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat)) {
            return true;
          }

          return std::nullopt;
        },
        [&](bool debounced) { rebuild |= debounced; });
  }
  return rebuild;
}
}  // namespace tr::renderer
