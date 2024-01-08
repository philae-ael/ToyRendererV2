#include "pipeline.h"

#include <utils/asset.h>
#include <utils/cast.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>

#include "device.h"
#include "utils.h"
#include "vertex.h"

auto create_shader_module(VkDevice device, std::span<const std::byte> module) -> VkShaderModule {
  VkShaderModule shader_module = VK_NULL_HANDLE;

  VkShaderModuleCreateInfo shader_module_create_info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .codeSize = utils::narrow_cast<uint32_t>(module.size()),
      // TODO: Do something for the 4 byte alignement requirement
      .pCode = reinterpret_cast<const uint32_t*>(module.data()),
  };

  VK_UNWRAP(vkCreateShaderModule, device, &shader_module_create_info, nullptr, &shader_module);
  return shader_module;
}

auto tr::renderer::Pipeline::init(Device& device, VkRenderPass renderpass) -> Pipeline {
  Pipeline p{};

  auto frag = read_file("./build/shaders/frag.spv");
  auto vert = read_file("./build/shaders/vert.spv");

  auto vert_shader_module = create_shader_module(device.vk_device, vert);
  auto frag_shader_module = create_shader_module(device.vk_device, frag);

  VkPipelineShaderStageCreateInfo vert_shader_stage_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vert_shader_module,
      .pName = "main",
      .pSpecializationInfo = nullptr,
  };
  VkPipelineShaderStageCreateInfo frag_shader_stage_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = frag_shader_module,
      .pName = "main",
      .pSpecializationInfo = nullptr,
  };

  std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{{
      frag_shader_stage_info,
      vert_shader_stage_info,
  }};

  std::array<VkDynamicState, 2> dynamic_states = {
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VIEWPORT,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .dynamicStateCount = utils::narrow_cast<uint32_t>(dynamic_states.size()),
      .pDynamicStates = dynamic_states.data(),
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .vertexBindingDescriptionCount = utils::narrow_cast<uint32_t>(Vertex::bindings.size()),
      .pVertexBindingDescriptions = Vertex::bindings.data(),
      .vertexAttributeDescriptionCount = utils::narrow_cast<uint32_t>(Vertex::attributes.size()),
      .pVertexAttributeDescriptions = Vertex::attributes.data(),
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport{0, 0, 100, 100, 1.0, 1.0};
  VkRect2D scissor{{0, 0}, {100, 100}};

  VkPipelineViewportStateCreateInfo viewport_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer_state{
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

  VkPipelineMultisampleStateCreateInfo multisampling_state{
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

  VkPipelineColorBlendAttachmentState color_blend_attchment_state{
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,

  };

  VkPipelineColorBlendStateCreateInfo color_blend_state{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attchment_state,
      .blendConstants = {0, 0, 0, 0},
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = 0,
      .pSetLayouts = nullptr,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };

  VK_UNWRAP(vkCreatePipelineLayout, device.vk_device, &pipeline_layout_info, nullptr, &p.layout);

  VkGraphicsPipelineCreateInfo pipeline_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stageCount = utils::narrow_cast<uint32_t>(shader_stages.size()),
      .pStages = shader_stages.data(),
      .pVertexInputState = &vertex_input_state,
      .pInputAssemblyState = &input_assembly_state,
      .pTessellationState = nullptr,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer_state,
      .pMultisampleState = &multisampling_state,
      .pDepthStencilState = nullptr,
      .pColorBlendState = &color_blend_state,
      .pDynamicState = &dynamic_state_state,
      .layout = p.layout,
      .renderPass = renderpass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  VK_UNWRAP(vkCreateGraphicsPipelines, device.vk_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &p.vk_pipeline);

  vkDestroyShaderModule(device.vk_device, vert_shader_module, nullptr);
  vkDestroyShaderModule(device.vk_device, frag_shader_module, nullptr);
  return p;
}
