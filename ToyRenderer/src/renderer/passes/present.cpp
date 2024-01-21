#include "present.h"

#include <vulkan/vulkan_core.h>

#include <array>
#include <glm/fwd.hpp>

#include "../pipeline.h"
#include "../vulkan_engine.h"

const std::array triangle_vert_bin = std::to_array<uint32_t>({
#include "shaders/present.vert.inc"
});

const std::array triangle_frag_bin = std::to_array<uint32_t>({
#include "shaders/present.frag.inc"
});

auto tr::renderer::Present::init(Lifetime &lifetime, VulkanContext &ctx, const RessourceManager &rm,
                                 Lifetime &setup_lifetime) -> Present {
  const auto frag = Shader::init_from_src(setup_lifetime, ctx.device.vk_device, triangle_frag_bin);
  const auto vert = Shader::init_from_src(setup_lifetime, ctx.device.vk_device, triangle_vert_bin);

  const auto shader_stages = std::to_array({
      frag.pipeline_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, "main"),
      vert.pipeline_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, "main"),
  });

  const std::array<VkDynamicState, 2> dynamic_states = {
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VIEWPORT,
  };
  const auto dynamic_state_state = PipelineDynamicStateBuilder{}.dynamic_state(dynamic_states).build();

  const auto vertex_input_state = PipelineVertexInputStateBuilder{}.build();
  const auto input_assembly_state =
      PipelineInputAssemblyBuilder{}.topology_(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST).build();
  const auto viewport_state = PipelineViewportStateBuilder{}.viewports_count(1).scissors_count(1).build();
  const auto rasterizer_state = PipelineRasterizationStateBuilder{}.build();
  const auto multisampling_state = PipelineMultisampleStateBuilder{}.build();
  const auto depth_state = DepthStateTestAndWriteOpLess.build();

  const auto color_blend_attchment_states = std::to_array<VkPipelineColorBlendAttachmentState>({
      PipelineColorBlendStateAllColorNoBlend.build(),
  });

  const auto color_formats = std::to_array<VkFormat>({
      rm.image_definition(ImageRessourceId::Swapchain).vk_format(ctx.swapchain),
  });

  const auto color_blend_state = PipelineColorBlendStateBuilder{}.attachments(color_blend_attchment_states).build();
  auto pipeline_rendering_create_info = PipelineRenderingBuilder{}.color_attachment_formats(color_formats).build();

  const auto descriptor_set_layouts = std::to_array({
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::Present::bindings).build(ctx.device.vk_device),
  });
  const auto layout = PipelineLayoutBuilder{}.set_layouts(descriptor_set_layouts).build(ctx.device.vk_device);
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

  VkSampler shadow_map_sampler = VK_NULL_HANDLE;
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
  VK_UNWRAP(vkCreateSampler, ctx.device.vk_device, &sampler_create_info, nullptr, &shadow_map_sampler);

  lifetime.tie(DeviceHandle::Pipeline, pipeline);
  lifetime.tie(DeviceHandle::PipelineLayout, layout);
  lifetime.tie(DeviceHandle::Sampler, shadow_map_sampler);
  for (auto descriptor_set_layout : descriptor_set_layouts) {
    lifetime.tie(DeviceHandle::DescriptorSetLayout, descriptor_set_layout);
  }

  return {descriptor_set_layouts, layout, pipeline, shadow_map_sampler};
}
void tr::renderer::Present::draw(Frame &frame, VkRect2D render_area) const {
  const DebugCmdScope scope(frame.cmd.vk_cmd, "Present");

  ImageMemoryBarrier::submit<2>(
      frame.cmd.vk_cmd,
      {{
          frame.frm.get_image(ImageRessourceId::Swapchain).invalidate().prepare_barrier(SyncColorAttachmentOutput),
          frame.frm.get_image(ImageRessourceId::Rendered).prepare_barrier(SyncFragmentStorageRead),
      }});
  std::array<VkRenderingAttachmentInfo, 1> attachments{
      frame.frm.get_image(ImageRessourceId::Swapchain).as_attachment(ImageClearOpDontCare{}),
  };

  const auto descriptor = frame.allocate_descriptor(descriptor_set_layouts[0]);
  DescriptorUpdater{descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
      .image_info({{
          {
              .sampler = scalling_sampler,
              .imageView = frame.frm.get_image(ImageRessourceId::Rendered).view,
              .imageLayout = SyncFragmentStorageRead.layout,
          },
      }})
      .write(frame.ctx->ctx.device.vk_device);

  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor, 0,
                          nullptr);

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
