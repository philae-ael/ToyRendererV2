#include "renderpass.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>

#include "swapchain.h"
#include "utils.h"

auto tr::renderer::Renderpass::init(VkDevice device, tr::renderer::Swapchain& swapchain) -> Renderpass {
  Renderpass renderpass;
  VkAttachmentDescription color_attachement{
      .flags = 0,
      .format = swapchain.surface_format.format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentReference color_attachement_ref{
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass{
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .pInputAttachments = nullptr,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachement_ref,
      .pResolveAttachments = nullptr,
      .pDepthStencilAttachment = nullptr,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = nullptr,
  };

  VkRenderPassCreateInfo renderpass_create_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .attachmentCount = 1,
      .pAttachments = &color_attachement,
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 0,
      .pDependencies = nullptr,
  };

  VK_UNWRAP(vkCreateRenderPass, device, &renderpass_create_info, nullptr, &renderpass.vk_renderpass);

  renderpass.framebuffers.resize(swapchain.image_views.size());
  for (std::size_t i = 0; i < swapchain.image_views.size(); i++) {
    std::array<VkImageView, 1> attachements{swapchain.image_views[i]};
    VkFramebufferCreateInfo framebuffer_create_info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = renderpass.vk_renderpass,
        .attachmentCount = utils::narrow_cast<uint32_t>(attachements.size()),
        .pAttachments = attachements.data(),
        .width = swapchain.extent.width,
        .height = swapchain.extent.height,
        .layers = 1,
    };

    VK_UNWRAP(vkCreateFramebuffer, device, &framebuffer_create_info, nullptr, &renderpass.framebuffers[i]);
  }
  return renderpass;
}
