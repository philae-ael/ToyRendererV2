#pragma once

#include <vulkan/vulkan_core.h>

#include <shaderc/shaderc.hpp>
#include <vector>

#include "../context.h"
#include "../pipeline.h"
#include "../ressource_manager.h"
#include "../ressources.h"

namespace tr::renderer {

// Vector can't be constexpr :-(
// Maybe use static_vector?

struct DepthAttachment {};
struct ColorAttachment {
  ImageRessourceDefinition def;
  VkPipelineColorBlendAttachmentState blend{};
};

struct PassInfo {
  std::vector<VkPipelineShaderStageCreateInfo> shaders;
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
  VkPipelineLayout pipeline_layout;

  struct Inputs {
    std::vector<image_ressource_handle> images;
    std::vector<buffer_ressource_handle> buffers;
  } inputs;

  struct Outputs {
    std::vector<image_ressource_handle> color_attachments;
    std::vector<VkFormat> color_attachment_formats;
    std::vector<VkPipelineColorBlendAttachmentState> color_attachment_blend_states;
    image_ressource_handle depth_attachement;
    VkFormat depth_attachement_format = VK_FORMAT_UNDEFINED;
    std::vector<buffer_ressource_handle> buffers;
  } outputs;
};

struct PassDefinition {
  std::vector<ShaderDefininition> shaders;
  std::vector<std::vector<VkDescriptorSetLayoutBinding>> descriptor_sets;
  std::vector<VkPushConstantRange> push_constants;

  struct Inputs {
    std::vector<ImageRessourceDefinition> images;
    std::vector<BufferRessourceDefinition> buffers;
  } inputs;

  struct Outputs {
    std::vector<ColorAttachment> color_attachments;
    std::optional<ImageRessourceDefinition> depth_attachement;
    std::vector<BufferRessourceDefinition> buffers;
  } outputs;

  auto build(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime,
             shaderc::Compiler &compiler, const shaderc::CompileOptions &options) const -> PassInfo;
};

struct BasicPipelineDefinition {
  VkPipelineVertexInputStateCreateInfo vertex_input_state;
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
  VkPipelineRasterizationStateCreateInfo rasterizer_state;
  VkPipelineDepthStencilStateCreateInfo depth_state;

  [[nodiscard]] auto build(Lifetime &lifetime, VulkanContext &ctx, const PassInfo &pass_info) const -> VkPipeline;
};
}  // namespace tr::renderer
