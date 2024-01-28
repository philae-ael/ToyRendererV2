#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <filesystem>
#include <shaderc/shaderc.hpp>
#include <unordered_set>
#include <utility>
#include <vector>

#include "deletion_stack.h"
#include "utils.h"
#include "utils/cast.h"
namespace tr::renderer {

struct Shader {
  static auto init_from_spv(Lifetime& lifetime, VkDevice, std::span<const uint32_t>) -> Shader;
  static auto compile(shaderc::Compiler& compiler, shaderc_shader_kind kind, const shaderc::CompileOptions& options,
                      const std::filesystem::path& path) -> std::optional<std::vector<uint32_t>>;
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

class FileIncluder : public shaderc::CompileOptions::IncluderInterface {
 public:
  explicit FileIncluder(std::filesystem::path base_path_) : base_path(std::move(base_path_)) {}
  auto GetInclude(const char* requested_source, shaderc_include_type include_type, const char* requesting_source,
                  size_t include_depth) -> shaderc_include_result* override;
  void ReleaseInclude(shaderc_include_result* include_result) override;

 private:
  [[nodiscard]] auto FindReadableFilepath(const std::string& filename) const -> std::string;
  auto FindRelativeReadableFilepath(const std::string& requesting_file, const std::string& filename) const
      -> std::string;

  std::filesystem::path base_path;
  struct FileInfo {
    std::string full_path;
    std::vector<char> contents;
  };
};

struct PipelineBuilder : VkBuilder<PipelineBuilder, VkGraphicsPipelineCreateInfo> {
  constexpr PipelineBuilder()
      : VkBuilder(VkGraphicsPipelineCreateInfo{
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
        }) {}

  constexpr auto layout_(VkPipelineLayout layout_) -> PipelineBuilder& {
    layout = layout_;
    return *this;
  }

  constexpr auto pipeline_rendering_create_info(VkPipelineRenderingCreateInfo* infos) -> PipelineBuilder& {
    return chain(infos);
  }

  constexpr auto stages(const std::span<const VkPipelineShaderStageCreateInfo> stages) -> PipelineBuilder& {
    stageCount = utils::narrow_cast<uint32_t>(stages.size());
    pStages = stages.data();
    return *this;
  }

  constexpr auto vertex_input_state(const VkPipelineVertexInputStateCreateInfo* infos) -> PipelineBuilder& {
    pVertexInputState = infos;
    return *this;
  }

  constexpr auto input_assembly_state(const VkPipelineInputAssemblyStateCreateInfo* infos) -> PipelineBuilder& {
    pInputAssemblyState = infos;
    return *this;
  }

  constexpr auto viewport_state(const VkPipelineViewportStateCreateInfo* infos) -> PipelineBuilder& {
    pViewportState = infos;
    return *this;
  }

  constexpr auto rasterization_state(const VkPipelineRasterizationStateCreateInfo* infos) -> PipelineBuilder& {
    pRasterizationState = infos;
    return *this;
  }

  constexpr auto multisample_state(const VkPipelineMultisampleStateCreateInfo* infos) -> PipelineBuilder& {
    pMultisampleState = infos;
    return *this;
  }

  constexpr auto depth_stencil_state(const VkPipelineDepthStencilStateCreateInfo* infos) -> PipelineBuilder& {
    pDepthStencilState = infos;
    return *this;
  }

  constexpr auto color_blend_state(const VkPipelineColorBlendStateCreateInfo* infos) -> PipelineBuilder& {
    pColorBlendState = infos;
    return *this;
  }

  constexpr auto dynamic_state(const VkPipelineDynamicStateCreateInfo* infos) -> PipelineBuilder& {
    pDynamicState = infos;
    return *this;
  }

  auto build(VkDevice& device) -> VkPipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;

    VK_UNWRAP(vkCreateGraphicsPipelines, device, VK_NULL_HANDLE, 1, inner_ptr(), nullptr, &pipeline);
    return pipeline;
  }
};

struct PipelineRenderingBuilder : VkBuilder<PipelineRenderingBuilder, VkPipelineRenderingCreateInfo> {
  constexpr PipelineRenderingBuilder()
      : VkBuilder({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext = nullptr,
            .viewMask = 0,
            .colorAttachmentCount = 0,
            .pColorAttachmentFormats = nullptr,
            .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
            .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
        }) {}

  constexpr auto color_attachment_formats(std::span<const VkFormat> formats) -> PipelineRenderingBuilder& {
    colorAttachmentCount = utils::narrow_cast<uint32_t>(formats.size());
    pColorAttachmentFormats = formats.data();
    return *this;
  }

  constexpr auto depth_attachment(VkFormat format) -> PipelineRenderingBuilder& {
    depthAttachmentFormat = format;
    return *this;
  }

  constexpr auto stencil_attachment(VkFormat format) -> PipelineRenderingBuilder& {
    depthAttachmentFormat = format;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineRenderingCreateInfo { return inner(); }
};

struct PipelineLayoutBuilder : VkBuilder<PipelineLayoutBuilder, VkPipelineLayoutCreateInfo> {
  constexpr PipelineLayoutBuilder()
      : VkBuilder({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        }) {}

  constexpr auto push_constant_ranges(std::span<const VkPushConstantRange> ranges) -> PipelineLayoutBuilder& {
    pushConstantRangeCount = utils::narrow_cast<uint32_t>(ranges.size());
    pPushConstantRanges = ranges.data();
    return *this;
  }

  constexpr auto set_layouts(std::span<const VkDescriptorSetLayout> layouts) -> PipelineLayoutBuilder& {
    setLayoutCount = utils::narrow_cast<uint32_t>(layouts.size());
    pSetLayouts = layouts.data();
    return *this;
  }

  auto build(VkDevice device) const -> VkPipelineLayout {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_UNWRAP(vkCreatePipelineLayout, device, inner_ptr(), nullptr, &layout);
    return layout;
  }
};

struct PipelineDepthStencilStateBuilder
    : VkBuilder<PipelineDepthStencilStateBuilder, VkPipelineDepthStencilStateCreateInfo> {
  constexpr PipelineDepthStencilStateBuilder()
      : VkBuilder({
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
        }) {}

  constexpr auto depth_test(bool enable) -> PipelineDepthStencilStateBuilder& {
    depthTestEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }

  constexpr auto depth_write(bool enable) -> PipelineDepthStencilStateBuilder& {
    depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }

  constexpr auto depth_compare_op(VkCompareOp op) -> PipelineDepthStencilStateBuilder& {
    depthCompareOp = op;
    return *this;
  }

  constexpr auto depth_bounds_test_enable(bool enable) -> PipelineDepthStencilStateBuilder& {
    depthBoundsTestEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineDepthStencilStateCreateInfo { return inner(); }
};

constexpr PipelineDepthStencilStateBuilder DepthStateTestReadOnlyOpLess =
    PipelineDepthStencilStateBuilder{}.depth_test(true).depth_write(false).depth_compare_op(VK_COMPARE_OP_LESS);

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

struct PipelineColorBlendAttachmentStateBuilder
    : VkBuilder<PipelineColorBlendAttachmentStateBuilder, VkPipelineColorBlendAttachmentState> {
  constexpr PipelineColorBlendAttachmentStateBuilder()
      : VkBuilder({
            .blendEnable = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = 0,
        }) {}

  constexpr auto blend(bool enable) -> PipelineColorBlendAttachmentStateBuilder& {
    blendEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }

  constexpr auto color_blend(VkBlendFactor src, VkBlendOp op, VkBlendFactor dst)
      -> PipelineColorBlendAttachmentStateBuilder& {
    srcColorBlendFactor = src;
    dstColorBlendFactor = dst;
    colorBlendOp = op;
    return *this;
  }

  constexpr auto alpha_blend(VkBlendFactor src, VkBlendOp op, VkBlendFactor dst)
      -> PipelineColorBlendAttachmentStateBuilder& {
    srcAlphaBlendFactor = src;
    dstAlphaBlendFactor = dst;
    alphaBlendOp = op;
    return *this;
  }

  constexpr auto color_write_mask(VkColorComponentFlags mask) -> PipelineColorBlendAttachmentStateBuilder& {
    colorWriteMask = mask;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineColorBlendAttachmentState { return inner(); }
};

constexpr PipelineColorBlendAttachmentStateBuilder PipelineColorBlendStateAllColorNoBlend =
    PipelineColorBlendAttachmentStateBuilder{}.color_write_mask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

constexpr PipelineColorBlendAttachmentStateBuilder PipelineColorBlendStateAllColorBlend =
    PipelineColorBlendAttachmentStateBuilder{}
        .color_write_mask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT)
        .color_blend(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
        .alpha_blend(VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO)
        .blend(true);

struct PipelineMultisampleStateBuilder
    : VkBuilder<PipelineMultisampleStateBuilder, VkPipelineMultisampleStateCreateInfo> {
  constexpr PipelineMultisampleStateBuilder()
      : VkBuilder({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        }) {}

  [[nodiscard]] auto build() const -> VkPipelineMultisampleStateCreateInfo { return inner(); }
};

struct PipelineRasterizationStateBuilder
    : VkBuilder<PipelineRasterizationStateBuilder, VkPipelineRasterizationStateCreateInfo> {
  constexpr PipelineRasterizationStateBuilder()
      : VkBuilder({
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
        }) {}

  constexpr auto polygone_mode(VkPolygonMode polygon_mode) -> PipelineRasterizationStateBuilder& {
    polygonMode = polygon_mode;
    return *this;
  }

  constexpr auto cull_mode(VkCullModeFlags cull_mode) -> PipelineRasterizationStateBuilder& {
    cullMode = cull_mode;
    return *this;
  }

  constexpr auto front_face(VkFrontFace front_face) -> PipelineRasterizationStateBuilder& {
    frontFace = front_face;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineRasterizationStateCreateInfo { return inner(); }
};

struct PipelineViewportStateBuilder : VkBuilder<PipelineViewportStateBuilder, VkPipelineViewportStateCreateInfo> {
  constexpr PipelineViewportStateBuilder()
      : VkBuilder({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 0,
            .pViewports = nullptr,
            .scissorCount = 0,
            .pScissors = nullptr,
        }) {}

  constexpr auto viewports(std::span<const VkViewport> viewports) -> PipelineViewportStateBuilder& {
    viewportCount = utils::narrow_cast<uint32_t>(viewports.size());
    pViewports = viewports.data();
    return *this;
  }

  constexpr auto scissors(std::span<const VkRect2D> scissors) -> PipelineViewportStateBuilder& {
    scissorCount = utils::narrow_cast<uint32_t>(scissors.size());
    pScissors = scissors.data();
    return *this;
  }

  constexpr auto viewports_count(uint32_t count) -> PipelineViewportStateBuilder& {
    viewportCount = count;
    return *this;
  }
  constexpr auto scissors_count(uint32_t count) -> PipelineViewportStateBuilder& {
    scissorCount = count;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineViewportStateCreateInfo { return inner(); }
};

struct PipelineInputAssemblyBuilder : VkBuilder<PipelineInputAssemblyBuilder, VkPipelineInputAssemblyStateCreateInfo> {
  constexpr PipelineInputAssemblyBuilder()
      : VkBuilder({

            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        }) {}

  constexpr auto topology_(VkPrimitiveTopology topology_) -> PipelineInputAssemblyBuilder& {
    topology = topology_;
    return *this;
  }

  constexpr auto primitive_restart(bool restart) -> PipelineInputAssemblyBuilder& {
    primitiveRestartEnable = restart ? VK_TRUE : VK_FALSE;
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineInputAssemblyStateCreateInfo { return inner(); }
};

struct PipelineVertexInputStateBuilder
    : VkBuilder<PipelineVertexInputStateBuilder, VkPipelineVertexInputStateCreateInfo> {
  constexpr PipelineVertexInputStateBuilder()
      : VkBuilder({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr,
        }) {}

  constexpr auto vertex_bindings(std::span<const VkVertexInputBindingDescription> bindings)
      -> PipelineVertexInputStateBuilder& {
    vertexBindingDescriptionCount = utils::narrow_cast<uint32_t>(bindings.size());
    pVertexBindingDescriptions = bindings.data();

    return *this;
  }

  constexpr auto vertex_attributes(std::span<const VkVertexInputAttributeDescription> attributes)
      -> PipelineVertexInputStateBuilder& {
    vertexAttributeDescriptionCount = utils::narrow_cast<uint32_t>(attributes.size());
    pVertexAttributeDescriptions = attributes.data();

    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineVertexInputStateCreateInfo { return inner(); }
};

struct PipelineDynamicStateBuilder : VkBuilder<PipelineDynamicStateBuilder, VkPipelineDynamicStateCreateInfo> {
  constexpr PipelineDynamicStateBuilder()
      : VkBuilder({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = 0,
            .pDynamicStates = nullptr,
        }) {}

  constexpr auto dynamic_state(std::span<const VkDynamicState> dynamic_state) -> PipelineDynamicStateBuilder {
    dynamicStateCount = utils::narrow_cast<uint32_t>(dynamic_state.size());
    pDynamicStates = dynamic_state.data();
    return *this;
  }

  [[nodiscard]] constexpr auto build() const -> VkPipelineDynamicStateCreateInfo { return inner(); }
};

}  // namespace tr::renderer
