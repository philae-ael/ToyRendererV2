#include "ssao.h"

#include <shaderc/env.h>         // for shaderc_env_version_vulkan_1_3
#include <shaderc/shaderc.h>     // for shaderc_glsl_fragment_shader
#include <vulkan/vulkan_core.h>  // for VkDescriptorType, VkShaderStage...

#include <array>                // for array, to_array
#include <cstdint>              // for uint32_t
#include <memory>               // for allocator, make_unique
#include <shaderc/shaderc.hpp>  // for CompileOptions, Compiler
#include <vector>               // for vector

#include "../buffer.h"                // for OneTimeCommandBuffer
#include "../context.h"               // for VulkanContext
#include "../debug.h"                 // for DebugCmdScope
#include "../deletion_stack.h"        // for DeviceHandle, Lifetime
#include "../descriptors.h"           // for DescriptorSetLayoutBindingBuilder
#include "../device.h"                // for Device
#include "../frame.h"                 // for Frame
#include "../pipeline.h"              // for ShaderDefininition, PipelineCol...
#include "../ressource_definition.h"  // for AO, DEPTH
#include "../ressource_manager.h"     // for FrameRessourceData
#include "../ressources.h"            // for ImageRessourceDefinition, Image...
#include "../synchronisation.h"       // for SyncFragmentShaderReadOnly, Syn...
#include "../utils.h"                 // for VK_UNWRAP
#include "../vulkan_engine.h"         // for VulkanEngine
#include "pass.h"                     // for ColorAttachment, PassInfo, Basi...
#include "utils/cast.h"               // for narrow_cast
#include "utils/types.h"              // for not_null_pointer

namespace tr::renderer {
constexpr std::array ssao_frag_spv = std::to_array<uint32_t>({
#include "shaders/ssao.frag.inc"  // IWYU pragma: keep
});
constexpr std::array ssao_vert_spv = std::to_array<uint32_t>({
#include "shaders/ssao.vert.inc"  // IWYU pragma: keep
});

const PassDefinition ssao_pass{
    .shaders =
        {
            ShaderDefininition{
                .kind = shaderc_glsl_fragment_shader,
                .entry_point = "main",
                .runtime_path = "ToyRenderer/shaders/ssao.frag",
                .compile_time_spv = {ssao_frag_spv.begin(), ssao_frag_spv.end()},
            },
            ShaderDefininition{
                .kind = shaderc_glsl_vertex_shader,
                .entry_point = "main",
                .runtime_path = "ToyRenderer/shaders/ssao.vert",
                .compile_time_spv = {ssao_vert_spv.begin(), ssao_vert_spv.end()},
            },
        },
    .descriptor_sets =
        {
            {
                DescriptorSetLayoutBindingBuilder{}
                    .binding_(0)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    .descriptor_count(1)
                    .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build(),
                DescriptorSetLayoutBindingBuilder{}
                    .binding_(1)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .descriptor_count(2)
                    .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .build(),
            },
        },
    .push_constants = {},
    .inputs =
        {
            .images = {GBUFFER_1, GBUFFER_3},
            .buffers = {CAMERA},
        },
    .outputs =
        {
            .color_attachments = {ColorAttachment{AO, PipelineColorBlendStateAllColorNoBlend.build()}},
            .depth_attachement = {},
            .buffers = {},
        },

};

constexpr BasicPipelineDefinition ssao_pipeline{
    .vertex_input_state = PipelineVertexInputStateBuilder{}.build(),
    .input_assembly_state = PipelineInputAssemblyBuilder{}.topology_(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST).build(),
    .rasterizer_state = PipelineRasterizationStateBuilder{}.build(),
    .depth_state = DepthStateTestReadOnlyOpLess.build(),
};

void SSAO::init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  options.SetIncluder(std::make_unique<FileIncluder>("./ToyRenderer/shaders"));
  options.SetGenerateDebugInfo();
  options.SetSourceLanguage(shaderc_source_language_glsl);
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);

  pass_info = ssao_pass.build(lifetime, ctx, rm, setup_lifetime, compiler, options);
  pipeline = ssao_pipeline.build(lifetime, ctx, pass_info);

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
  VK_UNWRAP(vkCreateSampler, ctx.device.vk_device, &sampler_create_info, nullptr, &sampler);
  lifetime.tie(DeviceHandle::Sampler, sampler);
}

void SSAO::draw(Frame &frame, VkRect2D render_area) const {
  const DebugCmdScope scope(frame.cmd.vk_cmd, "SSAO");
  ImageRessource &normal_ressource = frame.frm->get_image_ressource(pass_info.inputs.images[0]);
  ImageRessource &pos_ressource = frame.frm->get_image_ressource(pass_info.inputs.images[1]);
  ImageRessource &ao_ressource = frame.frm->get_image_ressource(pass_info.outputs.color_attachments[0]);

  ImageMemoryBarrier::submit<3>(frame.cmd.vk_cmd,
                                {{
                                    pos_ressource.prepare_barrier(SyncFragmentShaderReadOnly),
                                    normal_ressource.prepare_barrier(SyncFragmentShaderReadOnly),
                                    ao_ressource.invalidate().prepare_barrier(SyncColorAttachmentOutput),
                                }});
  std::array<VkRenderingAttachmentInfo, 1> attachments{
      ao_ressource.as_attachment(ImageClearOpDontCare{}),
  };

  const auto descriptor = frame.allocate_descriptor(pass_info.descriptor_set_layouts[0]);
  const auto &b = frame.frm->get_buffer_ressource(pass_info.inputs.buffers[0]);
  const VkDescriptorBufferInfo buffer_info{b.buffer, 0, b.size};
  DescriptorUpdater{descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .buffer_info(std::span{&buffer_info, 1})
      .write(frame.ctx->ctx.device.vk_device);
  DescriptorUpdater{descriptor, 1}
      .type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
      .image_info({{
          {
              .sampler = sampler,
              .imageView = normal_ressource.view,
              .imageLayout = SyncFragmentShaderReadOnly.layout,
          },
          {
              .sampler = sampler,
              .imageView = pos_ressource.view,
              .imageLayout = SyncFragmentShaderReadOnly.layout,
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
