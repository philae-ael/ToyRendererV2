#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <filesystem>

#include "deletion_queue.h"
#include "utils.h"
#include "utils/cast.h"
namespace tr::renderer {

struct Shader {
  static auto init_from_src(VkDevice, std::span<const uint32_t>) -> Shader;
  static auto init_from_filename(VkDevice, const std::filesystem::path&) -> Shader;

  void defer_deletion(DeviceDeletionStack& device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::ShaderModule, module);
  }

  VkShaderModule module;

  auto pipeline_shader_stage(VkShaderStageFlagBits stage, const char* entry_point) const
      -> VkPipelineShaderStageCreateInfo {
    return VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = stage,
        .module = module,
        .pName = entry_point,
        .pSpecializationInfo = nullptr,
    };
  }
};

struct PipelineBuilder {
  VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stageCount = 0,
      .pStages = nullptr,
      .pVertexInputState = nullptr,
      .pInputAssemblyState = nullptr,
      .pTessellationState = nullptr,
      .pViewportState = nullptr,
      .pRasterizationState = nullptr,
      .pMultisampleState = nullptr,
      .pDepthStencilState = nullptr,
      .pColorBlendState = nullptr,
      .pDynamicState = nullptr,
      .layout = VK_NULL_HANDLE,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  constexpr auto layout(VkPipelineLayout layout) -> PipelineBuilder& {
    graphics_pipeline_create_info.layout = layout;
    return *this;
  }

  // Can't constexpr without modifying pNext->pNext i guess
  /*constexpr*/ auto chain(const VkBaseInStructure* pNext) -> PipelineBuilder& {
    auto* next = reinterpret_cast<VkBaseOutStructure*>(&graphics_pipeline_create_info);
    while (next->pNext != nullptr) {
      next = next->pNext;
    }
    reinterpret_cast<VkBaseInStructure*>(next)->pNext = pNext;
    return *this;
  }

  auto pipeline_rendering_create_info(const VkPipelineRenderingCreateInfo* infos) -> PipelineBuilder& {
    return chain(reinterpret_cast<const VkBaseInStructure*>(infos));
  }

  constexpr auto stages(const std::span<const VkPipelineShaderStageCreateInfo> stages) {
    graphics_pipeline_create_info.stageCount = utils::narrow_cast<uint32_t>(stages.size());
    graphics_pipeline_create_info.pStages = stages.data();
    return *this;
  }

  constexpr auto vertex_input_state(const VkPipelineVertexInputStateCreateInfo* infos) {
    graphics_pipeline_create_info.pVertexInputState = infos;
    return *this;
  }

  constexpr auto input_assembly_state(const VkPipelineInputAssemblyStateCreateInfo* infos) {
    graphics_pipeline_create_info.pInputAssemblyState = infos;
    return *this;
  }

  constexpr auto viewport_state(const VkPipelineViewportStateCreateInfo* infos) {
    graphics_pipeline_create_info.pViewportState = infos;
    return *this;
  }

  constexpr auto rasterization_state(const VkPipelineRasterizationStateCreateInfo* infos) {
    graphics_pipeline_create_info.pRasterizationState = infos;
    return *this;
  }

  constexpr auto multisample_state(const VkPipelineMultisampleStateCreateInfo* infos) {
    graphics_pipeline_create_info.pMultisampleState = infos;
    return *this;
  }

  constexpr auto depth_stencil_state(const VkPipelineDepthStencilStateCreateInfo* infos) {
    graphics_pipeline_create_info.pDepthStencilState = infos;
    return *this;
  }

  constexpr auto color_blend_state(const VkPipelineColorBlendStateCreateInfo* infos) {
    graphics_pipeline_create_info.pColorBlendState = infos;
    return *this;
  }

  constexpr auto dynamic_state(const VkPipelineDynamicStateCreateInfo* infos) {
    graphics_pipeline_create_info.pDynamicState = infos;
    return *this;
  }

  [[nodiscard]] constexpr auto copy() const -> PipelineBuilder { return *this; }
  auto build(VkDevice& device) -> VkPipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;

    VK_UNWRAP(vkCreateGraphicsPipelines, device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pipeline);
    return pipeline;
  }
};

struct PipelineRenderingBuilder {
  VkPipelineRenderingCreateInfo pipeline_rendering_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .pNext = nullptr,
      .viewMask = 0,
      .colorAttachmentCount = 0,
      .pColorAttachmentFormats = nullptr,
      .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
  };

  constexpr auto color_attachment_formats(std::span<const VkFormat> formats) -> PipelineRenderingBuilder {
    pipeline_rendering_create_info.colorAttachmentCount = utils::narrow_cast<uint32_t>(formats.size());
    pipeline_rendering_create_info.pColorAttachmentFormats = formats.data();
    return *this;
  }

  constexpr auto depth_attachment(VkFormat format) -> PipelineRenderingBuilder {
    pipeline_rendering_create_info.depthAttachmentFormat = format;
    return *this;
  }

  constexpr auto stencil_attachment(VkFormat format) -> PipelineRenderingBuilder {
    pipeline_rendering_create_info.depthAttachmentFormat = format;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineRenderingCreateInfo { return pipeline_rendering_create_info; }
};

struct PipelineLayoutBuilder {
  VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = 0,
      .pSetLayouts = nullptr,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };

  constexpr auto push_constant_ranges(std::span<const VkPushConstantRange> ranges) -> PipelineLayoutBuilder {
    pipeline_layout_info.pushConstantRangeCount = utils::narrow_cast<uint32_t>(ranges.size());
    pipeline_layout_info.pPushConstantRanges = ranges.data();
    return *this;
  }

  constexpr auto set_layouts(std::span<const VkDescriptorSetLayout> layouts) -> PipelineLayoutBuilder {
    pipeline_layout_info.setLayoutCount = utils::narrow_cast<uint32_t>(layouts.size());
    pipeline_layout_info.pSetLayouts = layouts.data();
    return *this;
  }

  auto build(VkDevice device) const -> VkPipelineLayout {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_UNWRAP(vkCreatePipelineLayout, device, &pipeline_layout_info, nullptr, &layout);
    return layout;
  }
};

struct PipelineDepthStencilStateBuilder {
  VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_NEVER,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front = {},
      .back = {},
      .minDepthBounds = 0.0,
      .maxDepthBounds = 1.0,
  };

  constexpr auto depth_test(bool enable) -> PipelineDepthStencilStateBuilder& {
    pipeline_depth_stencil_state.depthTestEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }

  constexpr auto depth_write(bool enable) -> PipelineDepthStencilStateBuilder& {
    pipeline_depth_stencil_state.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }

  constexpr auto depth_compare_op(VkCompareOp op) -> PipelineDepthStencilStateBuilder& {
    pipeline_depth_stencil_state.depthCompareOp = op;
    return *this;
  }

  constexpr auto depth_bounds_test_enable(bool enable) -> PipelineDepthStencilStateBuilder& {
    pipeline_depth_stencil_state.depthBoundsTestEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineDepthStencilStateCreateInfo {
    return pipeline_depth_stencil_state;
  }
};

constexpr PipelineDepthStencilStateBuilder DepthStateTestAndWriteOpLess =
    PipelineDepthStencilStateBuilder{}.depth_test(true).depth_write(true).depth_compare_op(VK_COMPARE_OP_LESS);

struct PipelineColorBlendStateBuilder {
  VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_NO_OP,
      .attachmentCount = 0,
      .pAttachments = nullptr,
      .blendConstants = {0, 0, 0, 0},
  };

  constexpr auto attachments(std::span<const VkPipelineColorBlendAttachmentState> attachments)
      -> PipelineColorBlendStateBuilder& {
    pipeline_color_blend_state.attachmentCount = utils::narrow_cast<uint32_t>(attachments.size());
    pipeline_color_blend_state.pAttachments = attachments.data();
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineColorBlendStateCreateInfo {
    return pipeline_color_blend_state;
  }
};

struct PipelineColorBlendAttachmentStateBuilder {
  VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state{
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = 0,
  };

  constexpr auto blend(bool enable) -> PipelineColorBlendAttachmentStateBuilder& {
    pipeline_color_blend_attachment_state.blendEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }

  constexpr auto color_blend(VkBlendFactor src, VkBlendOp op, VkBlendFactor dst)
      -> PipelineColorBlendAttachmentStateBuilder& {
    pipeline_color_blend_attachment_state.srcColorBlendFactor = src;
    pipeline_color_blend_attachment_state.dstColorBlendFactor = dst;
    pipeline_color_blend_attachment_state.colorBlendOp = op;
    return *this;
  }

  constexpr auto alpha_blend(VkBlendFactor src, VkBlendOp op, VkBlendFactor dst)
      -> PipelineColorBlendAttachmentStateBuilder& {
    pipeline_color_blend_attachment_state.srcAlphaBlendFactor = src;
    pipeline_color_blend_attachment_state.dstAlphaBlendFactor = dst;
    pipeline_color_blend_attachment_state.alphaBlendOp = op;
    return *this;
  }

  constexpr auto color_write_mask(VkColorComponentFlags mask) -> PipelineColorBlendAttachmentStateBuilder& {
    pipeline_color_blend_attachment_state.colorWriteMask = mask;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineColorBlendAttachmentState {
    return pipeline_color_blend_attachment_state;
  }

  [[nodiscard]] constexpr auto copy() const -> PipelineColorBlendAttachmentStateBuilder { return *this; }
};

constexpr PipelineColorBlendAttachmentStateBuilder PipelineColorBlendStateAllColorNoBlend =
    PipelineColorBlendAttachmentStateBuilder{}.color_write_mask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

struct PipelineMultisampleStateBuilder {
  VkPipelineMultisampleStateCreateInfo pipeline_multisample_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };

  [[nodiscard]] auto build() const -> VkPipelineMultisampleStateCreateInfo { return pipeline_multisample_state; }
};

struct PipelineRasterizationStateBuilder {
  VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0,
      .depthBiasClamp = 0.0,
      .depthBiasSlopeFactor = 0.0,
      .lineWidth = 1.0,
  };

  constexpr auto polygone_mode(VkPolygonMode polygon_mode) -> PipelineRasterizationStateBuilder& {
    pipeline_rasterization_state.polygonMode = polygon_mode;
    return *this;
  }

  constexpr auto cull_mode(VkCullModeFlags cull_mode) -> PipelineRasterizationStateBuilder& {
    pipeline_rasterization_state.cullMode = cull_mode;
    return *this;
  }

  constexpr auto front_face(VkFrontFace front_face) -> PipelineRasterizationStateBuilder& {
    pipeline_rasterization_state.frontFace = front_face;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineRasterizationStateCreateInfo {
    return pipeline_rasterization_state;
  }
};

struct PipelineViewportStateBuilder {
  VkPipelineViewportStateCreateInfo pipeline_viewport_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .viewportCount = 0,
      .pViewports = nullptr,
      .scissorCount = 0,
      .pScissors = nullptr,
  };

  constexpr auto viewports(std::span<const VkViewport> viewports) -> PipelineViewportStateBuilder& {
    pipeline_viewport_state.viewportCount = utils::narrow_cast<uint32_t>(viewports.size());
    pipeline_viewport_state.pViewports = viewports.data();
    return *this;
  }

  constexpr auto scissors(std::span<const VkRect2D> scissors) -> PipelineViewportStateBuilder& {
    pipeline_viewport_state.scissorCount = utils::narrow_cast<uint32_t>(scissors.size());
    pipeline_viewport_state.pScissors = scissors.data();
    return *this;
  }

  constexpr auto viewports_count(uint32_t count) -> PipelineViewportStateBuilder& {
    pipeline_viewport_state.viewportCount = count;
    return *this;
  }
  constexpr auto scissors_count(uint32_t count) -> PipelineViewportStateBuilder& {
    pipeline_viewport_state.scissorCount = count;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineViewportStateCreateInfo { return pipeline_viewport_state; }
};

struct PipelineInputAssemblyBuilder {
  VkPipelineInputAssemblyStateCreateInfo pipeline_input_assemble_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  constexpr auto topology(VkPrimitiveTopology topology) -> PipelineInputAssemblyBuilder& {
    pipeline_input_assemble_state.topology = topology;
    return *this;
  }

  constexpr auto primitive_restart(bool restart) -> PipelineInputAssemblyBuilder& {
    pipeline_input_assemble_state.primitiveRestartEnable = restart ? VK_TRUE : VK_FALSE;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineInputAssemblyStateCreateInfo {
    return pipeline_input_assemble_state;
  }
};

struct PipelineVertexInputStateBuilder {
  VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr,
  };

  constexpr auto vertex_bindings(std::span<const VkVertexInputBindingDescription> bindings)
      -> PipelineVertexInputStateBuilder& {
    pipeline_vertex_input_state.vertexBindingDescriptionCount = utils::narrow_cast<uint32_t>(bindings.size());
    pipeline_vertex_input_state.pVertexBindingDescriptions = bindings.data();

    return *this;
  }

  constexpr auto vertex_attributes(std::span<const VkVertexInputAttributeDescription> attributes)
      -> PipelineVertexInputStateBuilder& {
    pipeline_vertex_input_state.vertexAttributeDescriptionCount = utils::narrow_cast<uint32_t>(attributes.size());
    pipeline_vertex_input_state.pVertexAttributeDescriptions = attributes.data();

    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineVertexInputStateCreateInfo {
    return pipeline_vertex_input_state;
  }
};

struct PipelineDynamicState {
  VkPipelineDynamicStateCreateInfo pipeline_dynamic_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .dynamicStateCount = 0,
      .pDynamicStates = nullptr,
  };

  constexpr auto dynamic_state(std::span<const VkDynamicState> dynamic_state) -> PipelineDynamicState {
    pipeline_dynamic_state.dynamicStateCount = utils::narrow_cast<uint32_t>(dynamic_state.size());
    pipeline_dynamic_state.pDynamicStates = dynamic_state.data();
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineDynamicStateCreateInfo { return pipeline_dynamic_state; }
};

}  // namespace tr::renderer
