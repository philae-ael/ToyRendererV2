#include "deferred.h"

#include <vulkan/vulkan_core.h>

#include <array>

#include "../debug.h"
#include "../descriptors.h"
#include "../pipeline.h"

const std::array triangle_vert_bin = std::to_array<uint32_t>({
#include "shaders/deferred.vert.inc"
});

const std::array triangle_frag_bin = std::to_array<uint32_t>({
#include "shaders/deferred.frag.inc"
});

auto tr::renderer::Deferred::init(VkDevice &device, Swapchain &swapchain, DeviceDeletionStack &device_deletion_stack)
    -> Deferred {
  const auto frag = Shader::init_from_src(device, triangle_frag_bin);
  const auto vert = Shader::init_from_src(device, triangle_vert_bin);
  frag.defer_deletion(device_deletion_stack);
  vert.defer_deletion(device_deletion_stack);

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
      attachments_definitions[0].format(swapchain),
  });

  const auto color_blend_state = PipelineColorBlendStateBuilder{}.attachments(color_blend_attchment_states).build();
  const auto pipeline_rendering_create_info =
      PipelineRenderingBuilder{}.color_attachment_formats(color_formats).build();

  const auto descriptor_set_layouts = std::to_array({
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::Deferred::bindings).build(device),
  });
  const auto layout = PipelineLayoutBuilder{}.set_layouts(descriptor_set_layouts).build(device);
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
                            .build(device);

  return {descriptor_set_layouts, layout, pipeline};
}
void tr::renderer::Deferred::draw(VkCommandBuffer cmd, FrameRessourceManager &rm, VkRect2D render_area) const {
  DebugCmdScope scope(cmd, "Deferred");

  ImageMemoryBarrier::submit<1>(cmd, {{
                                         rm.swapchain.invalidate().prepare_barrier(SyncColorAttachment),
                                     }});
  std::array<VkRenderingAttachmentInfo, 1> attachments{
      rm.swapchain.as_attachment(VkClearValue{.color = {.float32 = {0.0, 0.0, 1.0, 1.0}}}),
  };

  VkRenderingInfo render_info{
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
  vkCmdBeginRendering(cmd, &render_info);

  VkViewport viewport{
      0, 0, static_cast<float>(render_area.extent.width), static_cast<float>(render_area.extent.height), 0.0, 1.0,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &render_area);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);
}
