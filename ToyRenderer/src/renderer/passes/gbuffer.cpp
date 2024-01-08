#include "gbuffer.h"

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <glm/mat4x4.hpp>

#include "../pipeline.h"
#include "../vertex.h"

const std::array gbuffer_vert_bin = std::to_array<uint32_t>({
#include "shaders/gbuffer.vert.inc"
});

const std::array gbuffer_frag_bin = std::to_array<uint32_t>({
#include "shaders/gbuffer.frag.inc"
});

auto tr::renderer::GBuffer::init(VkDevice &device, Swapchain &swapchain, DeviceDeletionStack &device_deletion_stack)
    -> GBuffer {
  const auto frag = Shader::init_from_src(device, gbuffer_frag_bin);
  const auto vert = Shader::init_from_src(device, gbuffer_vert_bin);
  frag.defer_deletion(device_deletion_stack);
  vert.defer_deletion(device_deletion_stack);

  const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{{
      frag.pipeline_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, "main"),
      vert.pipeline_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, "main"),
  }};

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

  const std::array color_blend_attchment_states = std::to_array<VkPipelineColorBlendAttachmentState>({
      PipelineColorBlendStateAllColorNoBlend.build(),
      PipelineColorBlendStateAllColorNoBlend.build(),
  });

  const std::array color_formats = std::to_array<VkFormat>({
      definitions[0].format(swapchain),
      definitions[1].format(swapchain),
  });

  const auto color_blend_state = PipelineColorBlendStateBuilder{}.attachments(color_blend_attchment_states).build();
  const auto pipeline_rendering_create_info = PipelineRenderingBuilder{}
                                                  .color_attachment_formats(color_formats)
                                                  .depth_attachment(definitions[2].format(swapchain))
                                                  .build();

  const auto descriptor_set_layouts = std::to_array({
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::GBuffer::set_0).build(device),
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::GBuffer::set_1).build(device),
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
                          .build(device);
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
