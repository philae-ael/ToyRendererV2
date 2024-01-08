#include "ressources.h"

#include <vulkan/vulkan_core.h>

#include "debug.h"
#include "synchronisation.h"

auto tr::renderer::ImageRessource::as_attachment(std::optional<VkClearValue> clearValue) -> VkRenderingAttachmentInfo {
  return VkRenderingAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = nullptr,
      .imageView = view,
      .imageLayout = sync_info.layout,
      .resolveMode = VK_RESOLVE_MODE_NONE,
      .resolveImageView = VK_NULL_HANDLE,
      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .loadOp = clearValue.has_value() ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearValue.value_or(VkClearValue{}),
  };
}

auto tr::renderer::ImageRessource::prepare_barrier(SyncInfo dst) -> std::optional<VkImageMemoryBarrier2> {
  std::optional<VkImageMemoryBarrier2> barrier;

  if (dst.layout != sync_info.layout || dst.queueFamilyIndex != sync_info.queueFamilyIndex) {
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;
    if ((usage & ImageUsageBits::IMAGE_USAGE_COLOR_BIT) != 0) {
      aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if ((usage & ImageUsageBits::IMAGE_USAGE_SAMPLED_BIT) != 0) {
      aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if ((usage & ImageUsageBits::IMAGE_USAGE_DEPTH_BIT) != 0) {
      aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
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

auto tr::renderer::ImageRessource::from_external_image(VkImage image, VkImageView view, ImageUsage usage,
                                                       SyncInfo sync_info) -> ImageRessource {
  return {image, view, sync_info, nullptr, usage};
}
auto tr::renderer::ImageDefinition::format(const Swapchain& swapchain) const -> VkFormat {
  TR_ASSERT(
      std::popcount(
          (flags & (IMAGE_OPTION_FLAG_FORMAT_R8G8B8A8_UNORM_BIT | IMAGE_OPTION_FLAG_FORMAT_R32G32B32A32_SFLOAT_BIT |
                    IMAGE_OPTION_FLAG_FORMAT_D16_UNORM_BIT | IMAGE_OPTION_FLAG_FORMAT_SAME_AS_FRAMEBUFFER_BIT))) == 1,
      "exacly one format has to be specified");

  if ((flags & IMAGE_OPTION_FLAG_FORMAT_R8G8B8A8_UNORM_BIT) != 0) {
    return VK_FORMAT_R8G8B8A8_UNORM;
  }

  if ((flags & IMAGE_OPTION_FLAG_FORMAT_R32G32B32A32_SFLOAT_BIT) != 0) {
    return VK_FORMAT_R32G32B32_SFLOAT;
  }

  if ((flags & IMAGE_OPTION_FLAG_FORMAT_D16_UNORM_BIT) != 0) {
    return VK_FORMAT_D16_UNORM;
  }

  if ((flags & IMAGE_OPTION_FLAG_FORMAT_SAME_AS_FRAMEBUFFER_BIT) != 0) {
    return swapchain.surface_format.format;
  }

  TR_ASSERT(false, "unexpected");
}

auto tr::renderer::ImageDefinition::image_usage() const -> VkImageUsageFlags {
  VkImageUsageFlags ret = 0;
  if ((usage & IMAGE_USAGE_COLOR_BIT) != 0) {
    ret |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  if ((usage & IMAGE_USAGE_DEPTH_BIT) != 0) {
    ret |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }
  if ((usage & IMAGE_USAGE_SAMPLED_BIT) != 0) {
    ret |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if ((usage & IMAGE_USAGE_TRANSFER_DST_BIT) != 0) {
    ret |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  return ret;
}

auto tr::renderer::ImageDefinition::aspect_mask() const -> VkImageAspectFlags {
  VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;
  if ((usage & ImageUsageBits::IMAGE_USAGE_COLOR_BIT) != 0) {
    aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if ((usage & ImageUsageBits::IMAGE_USAGE_SAMPLED_BIT) != 0) {
    aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if ((usage & ImageUsageBits::IMAGE_USAGE_DEPTH_BIT) != 0) {
    aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  return aspectMask;
}

auto tr::renderer::ImageDefinition::extent(const Swapchain& swapchain) const -> VkExtent3D {
  if ((flags & IMAGE_OPTION_FLAG_SIZE_SAME_AS_FRAMEBUFFER_BIT) != 0) {
    return VkExtent3D{
        .width = swapchain.extent.width,
        .height = swapchain.extent.height,
        .depth = 1,
    };
  }
  if ((flags & IMAGE_OPTION_FLAG_SIZE_CUSTOM_BIT) != 0) {
    TR_ASSERT(size, " IMAGE_OPTION_FLAG_SIZE_CUSTOM_BIT is set but size is not set");
    return VkExtent3D{
        .width = size->width,
        .height = size->height,
        .depth = 1,
    };
  }

  TR_ASSERT(false, "No Flag for image size!");
}

auto tr::renderer::ImageBuilder::build_image(ImageDefinition definition, std::string_view debug_name) const
    -> ImageRessource {
  const auto format = definition.format(*swapchain);
  const auto aspect_mask = definition.aspect_mask();
  const auto extent = definition.extent(*swapchain);

  VkImageCreateInfo image_create_info{
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
      .usage = definition.image_usage(),
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  VmaAllocationCreateInfo allocation_create_info{};
  VkImage image = VK_NULL_HANDLE;
  VmaAllocation alloc{};
  VmaAllocationInfo alloc_info{};
  VK_UNWRAP(vmaCreateImage, allocator, &image_create_info, &allocation_create_info, &image, &alloc, &alloc_info);
  set_debug_object_name(device, VK_OBJECT_TYPE_IMAGE, image, std::format("{} image", debug_name));

  VkImageViewCreateInfo view_create_info{
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
  set_debug_object_name(device, VK_OBJECT_TYPE_IMAGE_VIEW, view, std::format("{} view", debug_name));

  return ImageRessource{
      .image = image,
      .view = view,
      .sync_info = SrcImageMemoryBarrierUndefined,
      .alloc = alloc,
      .usage = definition.usage,
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

auto tr::renderer::BufferBuilder::build_buffer(BufferDefinition definition, std::string_view debug_name) const
    -> BufferRessource {
  BufferRessource res{
      .usage = definition.usage,
      .size = definition.size,
  };

  VkBufferCreateInfo buffer_create_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = definition.size,
      .usage = definition.usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
  };

  VmaAllocationCreateInfo allocation_create_info{
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
  set_debug_object_name(device, VK_OBJECT_TYPE_BUFFER, res.buffer, std::format("{} buffer", debug_name));

  return res;
}
