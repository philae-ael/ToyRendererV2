#include "ressources.h"

#include <vulkan/vulkan_core.h>

#include <variant>

#include "debug.h"
#include "swapchain.h"
#include "synchronisation.h"
#include "utils/misc.h"

auto tr::renderer::ImageRessource::as_attachment(
    std::variant<VkClearValue, ImageClearOpLoad, ImageClearOpDontCare> clearOp) -> VkRenderingAttachmentInfo {
  return VkRenderingAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = nullptr,
      .imageView = view,
      .imageLayout = sync_info.layout,
      .resolveMode = VK_RESOLVE_MODE_NONE,
      .resolveImageView = VK_NULL_HANDLE,
      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .loadOp = std::visit(utils::overloaded{
                               [](VkClearValue) { return VK_ATTACHMENT_LOAD_OP_CLEAR; },
                               [](ImageClearOpLoad) { return VK_ATTACHMENT_LOAD_OP_LOAD; },
                               [](ImageClearOpDontCare) { return VK_ATTACHMENT_LOAD_OP_DONT_CARE; },
                           },
                           clearOp),
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = std::visit(utils::overloaded{
                                   [](VkClearValue v) { return v; },
                                   [](auto) { return VkClearValue{}; },
                               },
                               clearOp),
  };
}

auto tr::renderer::ImageRessource::prepare_barrier(SyncInfo dst) -> std::optional<VkImageMemoryBarrier2> {
  std::optional<VkImageMemoryBarrier2> barrier;

  if (dst.layout != sync_info.layout || dst.queueFamilyIndex != sync_info.queueFamilyIndex) {
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;
    if ((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0) {
      aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0) {
      aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if ((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
      // WARN: this is an hack ! It seems aspect can't be deduced, it should be given
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    barrier = sync_info.barrier(dst, image,
                                VkImageSubresourceRange{
                                    .aspectMask = aspectMask,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1,
                                });
  }

  sync_info = SyncInfo{
      .accessMask = dst.accessMask,
      .stageMask = dst.stageMask,
      .layout = dst.layout,
      .queueFamilyIndex = dst.queueFamilyIndex,
  };

  return barrier;
}

auto tr::renderer::ImageRessource::invalidate() -> ImageRessource& {
  sync_info = SrcImageMemoryBarrierUndefined;
  return *this;
}

auto tr::renderer::ImageRessource::from_external_image(VkImage image, VkImageView view, VkImageUsageFlags usage,
                                                       VkExtent2D extent, SyncInfo sync_info) -> ImageRessource {
  return {image, view, sync_info, nullptr, usage, extent};
}
auto tr::renderer::ImageDefinition::vk_format(const Swapchain& swapchain) const -> VkFormat {
  return std::visit(utils::overloaded{
                        [&](FramebufferFormat) { return swapchain.surface_format.format; },
                        [](VkFormat format) { return format; },
                    },
                    format);
}

auto tr::renderer::ImageDefinition::vk_aspect_mask() const -> VkImageAspectFlags {
  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;
  if ((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0) {
    aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0) {
    aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if ((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
    // WARN: this is an hack ! It seems aspect can't be deduced, it should be given
    aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  return aspectMask;
}

auto tr::renderer::ImageDefinition::vk_extent(const Swapchain& swapchain) const -> VkExtent3D {
  return std::visit(utils::overloaded{[&](FramebufferExtent) {
                                        return VkExtent3D{
                                            .width = swapchain.extent.width,
                                            .height = swapchain.extent.height,
                                            .depth = 1,
                                        };
                                      },
                                      [](VkExtent2D extent) {
                                        return VkExtent3D{
                                            .width = extent.width,
                                            .height = extent.height,
                                            .depth = 1,
                                        };
                                      }},
                    size);
}

auto tr::renderer::ImageBuilder::build_image(ImageDefinition definition) const -> ImageRessource {
  const auto format = definition.vk_format(*swapchain);
  const auto aspect_mask = definition.vk_aspect_mask();
  const auto extent = definition.vk_extent(*swapchain);

  const VkImageCreateInfo image_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = extent,
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = definition.usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  const VmaAllocationCreateInfo allocation_create_info{
      .flags = 0,
      .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      .requiredFlags = 0,
      .preferredFlags = 0,
      .memoryTypeBits = 0,
      .pool = nullptr,
      .pUserData = nullptr,
      .priority = 0,
  };
  VkImage image = VK_NULL_HANDLE;
  VmaAllocation alloc{};
  VmaAllocationInfo alloc_info{};
  VK_UNWRAP(vmaCreateImage, allocator, &image_create_info, &allocation_create_info, &image, &alloc, &alloc_info);
  set_debug_object_name(device, VK_OBJECT_TYPE_IMAGE, image, std::format("{} image", definition.debug_name));

  const VkImageViewCreateInfo view_create_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components =
          {
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
          },
      .subresourceRange =
          {
              .aspectMask = aspect_mask,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };
  VkImageView view = VK_NULL_HANDLE;
  VK_UNWRAP(vkCreateImageView, device, &view_create_info, nullptr, &view);
  set_debug_object_name(device, VK_OBJECT_TYPE_IMAGE_VIEW, view, std::format("{} view", definition.debug_name));

  return ImageRessource{
      .image = image,
      .view = view,
      .sync_info = SrcImageMemoryBarrierUndefined,
      .alloc = alloc,
      .usage = definition.usage,
      .extent = {.width = extent.width, .height = extent.height},
  };
}

auto tr::renderer::BufferDefinition::vma_required_flags() const -> VkMemoryPropertyFlags {
  if ((flags & BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT) != 0) {
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  }
  return 0;
}

auto tr::renderer::BufferDefinition::vma_prefered_flags() const -> VkMemoryPropertyFlags {
  if ((flags & BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT) != 0) {
    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  }
  return 0;
}

auto tr::renderer::BufferDefinition::vma_usage() const -> VmaMemoryUsage {
  if ((flags & BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT) != 0) {
    return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  }
  return VMA_MEMORY_USAGE_AUTO;
}

auto tr::renderer::BufferDefinition::vma_flags() const -> VmaAllocationCreateFlags {
  if ((flags & BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT) != 0) {
    return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
  }
  return 0;
}

auto tr::renderer::BufferBuilder::build_buffer(BufferDefinition definition) const -> BufferRessource {
  BufferRessource res{
      .usage = definition.usage,
      .size = definition.size,
  };

  const VkBufferCreateInfo buffer_create_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = definition.size,
      .usage = definition.usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
  };

  const VmaAllocationCreateInfo allocation_create_info{
      .flags = definition.vma_flags(),
      .usage = definition.vma_usage(),
      .requiredFlags = definition.vma_required_flags(),
      .preferredFlags = definition.vma_prefered_flags(),
      .memoryTypeBits = 0,
      .pool = VK_NULL_HANDLE,
      .pUserData = nullptr,
      .priority = 1.0F,
  };
  VK_UNWRAP(vmaCreateBuffer, allocator, &buffer_create_info, &allocation_create_info, &res.buffer, &res.alloc, nullptr);
  set_debug_object_name(device, VK_OBJECT_TYPE_BUFFER, res.buffer, std::format("{} buffer", definition.debug_name));

  return res;
}

auto tr::renderer::RessourceManager::frame(uint32_t frame_index) -> FrameRessourceManager {
  return {
      this,
      frame_index,
  };
}
