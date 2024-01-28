#include "debug.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <cstring>
#include <shaderc/shaderc.hpp>

#include "../pipeline.h"
#include "../vulkan_engine.h"
#include "utils/assert.h"
#include "utils/cast.h"

const std::array vert_spv_default = std::to_array<uint32_t>({
#include "shaders/debug.vert.inc"
});

const std::array frag_spv_default = std::to_array<uint32_t>({
#include "shaders/debug.frag.inc"
});

void tr::renderer::Debug::init(Lifetime &lifetime, VulkanContext &ctx, const RessourceManager &rm,
                               Lifetime &setup_lifetime) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetGenerateDebugInfo();
  options.SetSourceLanguage(shaderc_source_language_glsl);
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);

  const auto shader_stages = TIMED_INLINE_LAMBDA("Compiling debug shader") {
    const auto frag_spv =
        Shader::compile(compiler, shaderc_glsl_fragment_shader, options, "./ToyRenderer/shaders/debug.frag");
    const auto vert_spv =
        Shader::compile(compiler, shaderc_glsl_vertex_shader, options, "./ToyRenderer/shaders/debug.vert");

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

  const auto vertex_input_state = PipelineVertexInputStateBuilder{}
                                      .vertex_attributes(DebugVertex::attributes)
                                      .vertex_bindings(DebugVertex::bindings)
                                      .build();

  const auto input_assembly_state =
      PipelineInputAssemblyBuilder{}.topology_(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST).build();
  const auto viewport_state = PipelineViewportStateBuilder{}.viewports_count(1).scissors_count(1).build();
  const auto rasterizer_state = PipelineRasterizationStateBuilder{}.cull_mode(VK_CULL_MODE_NONE).build();
  const auto multisampling_state = PipelineMultisampleStateBuilder{}.build();
  const auto depth_state = DepthStateTestReadOnlyOpLess.build();

  const std::array color_blend_attchment_states = std::to_array<VkPipelineColorBlendAttachmentState>({
      PipelineColorBlendStateAllColorBlend.build(),
  });

  const std::array<VkFormat, 1> color_formats{
      rm.image_definition(ImageRessourceId::Rendered).vk_format(ctx.swapchain),
  };

  const auto color_blend_state = PipelineColorBlendStateBuilder{}.attachments(color_blend_attchment_states).build();
  auto pipeline_rendering_create_info =
      PipelineRenderingBuilder{}
          .color_attachment_formats(color_formats)
          .depth_attachment(rm.image_definition(ImageRessourceId::Depth).vk_format(ctx.swapchain))
          .build();

  descriptor_set_layouts = std::to_array({
      tr::renderer::DescriptorSetLayoutBuilder{}.bindings(tr::renderer::Debug::set_0).build(ctx.device.vk_device),
  });

  pipeline_layout = PipelineLayoutBuilder{}.set_layouts(descriptor_set_layouts).build(ctx.device.vk_device);
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

  lifetime.tie(DeviceHandle::Pipeline, pipeline);
  lifetime.tie(DeviceHandle::PipelineLayout, pipeline_layout);
  for (auto descriptor_set_layout : descriptor_set_layouts) {
    lifetime.tie(DeviceHandle::DescriptorSetLayout, descriptor_set_layout);
  }
}

void tr::renderer::Debug::draw(Frame &frame, VkRect2D render_area) {
  if (vertices.empty()) {
    return;
  }
  const DebugCmdScope scope(frame.cmd.vk_cmd, "Debug");
  ImageMemoryBarrier::submit<2>(
      frame.cmd.vk_cmd, {{
                            frame.frm.get_image(ImageRessourceId::Depth).prepare_barrier(SyncLateDepthReadOnly),
                            frame.frm.get_image(ImageRessourceId::Rendered).prepare_barrier(SyncColorAttachmentOutput),
                        }});

  const std::array attachments = utils::to_array<VkRenderingAttachmentInfo>({
      frame.frm.get_image(ImageRessourceId::Rendered).as_attachment(ImageClearOpLoad{}),
  });

  const VkRenderingAttachmentInfo depthAttachment =
      frame.frm.get_image(ImageRessourceId::Depth).as_attachment(ImageClearOpLoad{});

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

  const auto &b = frame.frm.get_buffer(BufferRessourceId::Camera);
  const VkDescriptorBufferInfo buffer_info{b.buffer, 0, b.size};

  const auto camera_descriptor = frame.allocate_descriptor(descriptor_set_layouts[0]);
  DescriptorUpdater{camera_descriptor, 0}
      .type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      .buffer_info({&buffer_info, 1})
      .write(frame.ctx->ctx.device.vk_device);

  vkCmdBindDescriptorSets(frame.cmd.vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &camera_descriptor,
                          0, nullptr);

  const VkDeviceSize offset = 0;
  const auto buf = frame.frm.get_buffer(BufferRessourceId::DebugVertices);

  TR_ASSERT(buf.mapped_data != nullptr, "debug vertices are not mapped");
  std::memcpy(buf.mapped_data, vertices.data(),
              std::min(buf.size, utils::narrow_cast<uint32_t>(vertices.size() * sizeof(vertices[0]))));

  vkCmdBindVertexBuffers(frame.cmd.vk_cmd, 0, 1, &buf.buffer, &offset);
  vkCmdDraw(frame.cmd.vk_cmd, utils::narrow_cast<uint32_t>(vertices.size()), 1, 0, 0);
  vkCmdEndRendering(frame.cmd.vk_cmd);

  vertices.clear();
}
