
#include "pass.h"

#include <vulkan/vulkan_core.h>

#include <array>
#include <ranges>
#include <span>

#include "../context.h"
#include "../deletion_stack.h"
#include "../descriptors.h"
#include "../device.h"
#include "../pipeline.h"
#include "../ressource_manager.h"
#include "../ressources.h"
#include "utils/misc.h"
#include "utils/timer.h"

namespace shaderc {

class CompileOptions;
class Compiler;
}  // namespace shaderc

auto tr::renderer::PassDefinition::build(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm,
                                         Lifetime &setup_lifetime, shaderc::Compiler &compiler,
                                         const shaderc::CompileOptions &options) const -> PassInfo {
  PassInfo infos;

  infos.shaders = TIMED_INLINE_LAMBDA("Compiling deferred shader") {
    const auto r = shaders | std::views::transform([&](const ShaderDefininition &def) {
                     return def.pipeline_shader_stage(setup_lifetime, ctx.device.vk_device, compiler, options);
                   });
    return std::vector<VkPipelineShaderStageCreateInfo>{r.begin(), r.end()};
  };

  infos.descriptor_set_layouts = INLINE_LAMBDA {
    const auto r = descriptor_sets | std::views::transform([&](std::span<const VkDescriptorSetLayoutBinding> bindings) {
                     const auto layout = DescriptorSetLayoutBuilder{}.bindings(bindings).build(ctx.device.vk_device);

                     lifetime.tie(DeviceHandle::DescriptorSetLayout, layout);
                     return layout;
                   });
    return std::vector<VkDescriptorSetLayout>{r.begin(), r.end()};
  };

  infos.pipeline_layout = PipelineLayoutBuilder{}
                              .set_layouts(infos.descriptor_set_layouts)
                              .push_constant_ranges(push_constants)
                              .build(ctx.device.vk_device);
  lifetime.tie(DeviceHandle::PipelineLayout, infos.pipeline_layout);

  infos.inputs.images = INLINE_LAMBDA {
    const auto r = inputs.images |
                   std::views::transform([&](const ImageRessourceDefinition &def) { return rm.register_image(def); });
    return std::vector<image_ressource_handle>{r.begin(), r.end()};
  };
  infos.inputs.buffers = INLINE_LAMBDA {
    const auto r = inputs.buffers |
                   std::views::transform([&](const BufferRessourceDefinition &def) { return rm.register_buffer(def); });
    return std::vector<buffer_ressource_handle>{r.begin(), r.end()};
  };

  infos.outputs.color_attachments = INLINE_LAMBDA {
    const auto r = outputs.color_attachments | std::views::transform([&](const ColorAttachment &attachment) {
                     return rm.register_image(attachment.def);
                   });
    return std::vector<image_ressource_handle>{r.begin(), r.end()};
  };
  infos.outputs.color_attachment_blend_states = INLINE_LAMBDA {
    const auto r = outputs.color_attachments |
                   std::views::transform([&](const ColorAttachment &attachment) { return attachment.blend; });
    return std::vector<VkPipelineColorBlendAttachmentState>{r.begin(), r.end()};
  };
  infos.outputs.color_attachment_formats = INLINE_LAMBDA {
    const auto r = outputs.color_attachments | std::views::transform([&](const ColorAttachment &attachment) {
                     return attachment.def.definition.vk_format(ctx.swapchain);
                   });
    return std::vector<VkFormat>{r.begin(), r.end()};
  };
  if (outputs.depth_attachement) {
    infos.outputs.depth_attachement = rm.register_image(*outputs.depth_attachement);
    infos.outputs.depth_attachement_format = outputs.depth_attachement->definition.vk_format(ctx.swapchain);
  }
  infos.outputs.buffers = INLINE_LAMBDA {
    const auto r = outputs.buffers |
                   std::views::transform([&](const BufferRessourceDefinition &def) { return rm.register_buffer(def); });
    return std::vector<buffer_ressource_handle>{r.begin(), r.end()};
  };

  return infos;
}
auto tr::renderer::BasicPipelineDefinition::build(Lifetime &lifetime, VulkanContext &ctx,
                                                  const PassInfo &pass_info) const -> VkPipeline {
  const std::array<VkDynamicState, 2> dynamic_states = {
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VIEWPORT,
  };
  const auto dynamic_state_state = PipelineDynamicStateBuilder{}.dynamic_state(dynamic_states).build();
  const auto viewport_state = PipelineViewportStateBuilder{}.viewports_count(1).scissors_count(1).build();
  const auto multisampling_state = PipelineMultisampleStateBuilder{}.build();

  const auto color_blend_state =
      PipelineColorBlendStateBuilder{}.attachments(pass_info.outputs.color_attachment_blend_states).build();
  auto pipeline_rendering_create_info = PipelineRenderingBuilder{}
                                            .color_attachment_formats(pass_info.outputs.color_attachment_formats)
                                            .depth_attachment(pass_info.outputs.depth_attachement_format)
                                            .build();

  VkPipeline pipeline = PipelineBuilder{}
                            .stages(pass_info.shaders)
                            .layout_(pass_info.pipeline_layout)
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
  return pipeline;
}
