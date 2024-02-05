#include "present.h"

#include <shaderc/env.h>         // for shaderc_env_version_vulkan_1_3
#include <shaderc/shaderc.h>     // for shaderc_glsl_fragment_shader
#include <vulkan/vulkan_core.h>  // for VkDescriptorType, VkShaderStage...

#include <algorithm>            // for copy, max
#include <array>                // for array, to_array
#include <cstdint>              // for uint32_t
#include <filesystem>           // for path
#include <optional>             // for optional
#include <shaderc/shaderc.hpp>  // for CompileOptions, Compiler
#include <vector>               // for vector, allocator

#include "../buffer.h"                // for OneTimeCommandBuffer
#include "../context.h"               // for VulkanContext
#include "../debug.h"                 // for DebugCmdScope
#include "../deletion_stack.h"        // for DeviceHandle, Lifetime
#include "../descriptors.h"           // for DescriptorSetLayoutBindingBuilder
#include "../device.h"                // for Device
#include "../frame.h"                 // for Frame
#include "../pipeline.h"              // for ShaderDefininition, PipelineCol...
#include "../ressource_definition.h"  // for SWAPCHAIN, RENDERED, DEPTH
#include "../ressource_manager.h"     // for FrameRessourceData
#include "../ressources.h"            // for ImageRessourceDefinition, Image...
#include "../synchronisation.h"       // for SyncFragmentStorageRead, SyncInfo
#include "../utils.h"                 // for VK_UNWRAP
#include "../vulkan_engine.h"         // for VulkanEngine
#include "pass.h"                     // for ColorAttachment, PassInfo, Basi...
#include "utils/cast.h"               // for narrow_cast
#include "utils/types.h"              // for not_null_pointer

namespace tr::renderer {
constexpr std::array present_vert_spv = std::to_array<uint32_t>({
#include "shaders/present.vert.inc"  // IWYU pragma: keep
});

constexpr std::array present_frag_spv = std::to_array<uint32_t>({
#include "shaders/present.frag.inc"  // IWYU pragma: keep
});

const PassDefinition present_pass{
    .shaders =
        {
            ShaderDefininition{
                .kind = shaderc_glsl_fragment_shader,
                .entry_point = "main",
                .runtime_path = "./ToyRenderer/shaders/present.frag",
                .compile_time_spv = {present_frag_spv.begin(), present_frag_spv.end()},
            },
            ShaderDefininition{
                .kind = shaderc_glsl_vertex_shader,
                .entry_point = "main",
                .runtime_path = "./ToyRenderer/shaders/present.vert",
                .compile_time_spv = {present_vert_spv.begin(), present_vert_spv.end()},
            },
        },
    .descriptor_sets =
        {
            {
                DescriptorSetLayoutBindingBuilder{}
                    .binding_(0)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .descriptor_count(1)
                    .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build(),
            },
        },
    .push_constants = {},
    .inputs =
        {
            .images = {RENDERED},
            .buffers = {},
        },
    .outputs =
        {
            .color_attachments =
                {
                    ColorAttachment{SWAPCHAIN, PipelineColorBlendStateAllColorNoBlend.build()},
                },
            .depth_attachement = DEPTH,
            .buffers = {},
        },
};

constexpr BasicPipelineDefinition present_pipeline{
    .vertex_input_state = PipelineVertexInputStateBuilder{}.build(),
    .input_assembly_state = PipelineInputAssemblyBuilder{}.topology_(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST).build(),
    .rasterizer_state = PipelineRasterizationStateBuilder{}.build(),
    .depth_state = DepthStateTestAndWriteOpLess.build(),
};

void Present::init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetGenerateDebugInfo();
  options.SetSourceLanguage(shaderc_source_language_glsl);
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);

  pass_info = present_pass.build(lifetime, ctx, rm, setup_lifetime, compiler, options);
  pipeline = present_pipeline.build(lifetime, ctx, pass_info);

  const VkSamplerCreateInfo sampler_create_info{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias = 0,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_NEVER,
      .minLod = 0,
      .maxLod = 0,
      .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
  };
  VK_UNWRAP(vkCreateSampler, ctx.device.vk_device, &sampler_create_info, nullptr, &sampler);
  lifetime.tie(DeviceHandle::Sampler, sampler);
}

void Present::draw(Frame &frame, VkRect2D render_area) const {
  const DebugCmdScope scope(frame.cmd.vk_cmd, "Present");

  auto &rendered = frame.frm->get_image_ressource(pass_info.inputs.images[0]);
  auto &swapchain = frame.frm->get_image_ressource(pass_info.outputs.color_attachments[0]);

  ImageMemoryBarrier::submit<2>(frame.cmd.vk_cmd, {{
                                                      swapchain.invalidate().prepare_barrier(SyncColorAttachmentOutput),
                                                      rendered.prepare_barrier(SyncFragmentStorageRead),
                                                  }});
  std::array<VkRenderingAttachmentInfo, 1> attachments{
      swapchain.as_attachment(ImageClearOpDontCare{}),
  };

  const auto descriptor = frame.allocate_descriptor(pass_info.descriptor_set_layouts[0]);
  DescriptorUpdater{descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
      .image_info({{
          {
              .sampler = sampler,
              .imageView = rendered.view,
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

  vkCmdDraw(frame.cmd.vk_cmd, 3, 1, 0, 0);

  vkCmdEndRendering(frame.cmd.vk_cmd);
}

}  // namespace tr::renderer
