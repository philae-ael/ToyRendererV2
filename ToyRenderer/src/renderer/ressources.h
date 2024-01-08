#pragma once
#include <utils/assert.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <bit>
#include <cstdint>
#include <format>
#include <optional>
#include <string_view>

#include "constants.h"
#include "debug.h"
#include "deletion_queue.h"
#include "swapchain.h"
#include "synchronisation.h"
#include "utils.h"

namespace tr::renderer {

enum class ImageUsage {
  Color,
  Depth,
};

struct ImageRessource {
  VkImage image;
  VkImageView view;
  SyncInfo sync_info;
  VmaAllocation alloc;
  ImageUsage usage;

  static auto from_external_image(VkImage image, VkImageView view, ImageUsage usage,
                                  SyncInfo sync_info = SrcImageMemoryBarrierUndefined) -> ImageRessource {
    return {image, view, sync_info, nullptr, usage};
  }

  auto invalidate() -> ImageRessource& {
    sync_info = SrcImageMemoryBarrierUndefined;
    return *this;
  }
  auto sync(VkCommandBuffer cmd, SyncInfo dst) -> ImageRessource& {
    if (dst.layout != sync_info.layout || dst.queueFamilyIndex != sync_info.queueFamilyIndex) {
      VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;
      switch (usage) {
        case ImageUsage::Color:
          aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          break;
        case ImageUsage::Depth:
          aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
          break;
      }
      sync_info
          .prepare_sync_transition(dst, image,
                                   VkImageSubresourceRange{
                                       .aspectMask = aspectMask,
                                       .baseMipLevel = 0,
                                       .levelCount = 1,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1,
                                   })
          .submit(cmd);
    }
    sync_info = SyncInfo{
        .accessMask = dst.accessMask,
        .stageMask = dst.stageMask,
        .layout = dst.layout,
        .queueFamilyIndex = dst.queueFamilyIndex,
    };
    return *this;
  }

  auto as_attachment(std::optional<VkClearValue> clearValue) -> VkRenderingAttachmentInfo {
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
};

// The ressources used during one frame
struct FrameRessourceManager {
  ImageRessource swapchain;
  ImageRessource depth;
  ImageRessource fb0;
  ImageRessource fb1;
};

enum ImageOptionsFlagBits {
  IMAGE_OPTION_FLAG_SIZE_SAME_AS_FRAMEBUFFER_BIT = 1 << 0,
  IMAGE_OPTION_FLAG_FORMAT_B8G8R8A8_UNORM_BIT = 1 << 1,  // VK_FORMAT_R8G8B8A8_UINT
  IMAGE_OPTION_FLAG_FORMAT_R32G32B32A32_SFLOAT_BIT = 1 << 2,
  IMAGE_OPTION_FLAG_FORMAT_D16_UNORM_BIT = 1 << 3,  // VK_FORMAT_D32_SFLOAT
  IMAGE_OPTION_FLAG_FORMAT_SAME_AS_FRAMEBUFFER_BIT = 1 << 4,
  IMAGE_OPTION_FLAG_COLOR_ATTACHMENT_BIT = 1 << 5,
  IMAGE_OPTION_FLAG_TEXTURE_ATTACHMENT_BIT = 1 << 6,
};
using ImageOptionsFlags = std::uint32_t;

struct ImageDefinition {
  ImageOptionsFlags flags;
  ImageUsage usage;

  auto format(Swapchain& swapchain) const -> VkFormat {
    TR_ASSERT(
        std::popcount(
            (flags & (IMAGE_OPTION_FLAG_FORMAT_B8G8R8A8_UNORM_BIT | IMAGE_OPTION_FLAG_FORMAT_R32G32B32A32_SFLOAT_BIT |
                      IMAGE_OPTION_FLAG_FORMAT_D16_UNORM_BIT | IMAGE_OPTION_FLAG_FORMAT_SAME_AS_FRAMEBUFFER_BIT))) == 1,
        "exacly one format has to be specified");

    if ((flags & IMAGE_OPTION_FLAG_FORMAT_B8G8R8A8_UNORM_BIT) != 0) {
      TR_ASSERT(usage == ImageUsage::Color, "BAD USAGE");
      return VK_FORMAT_B8G8R8A8_UNORM;
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

  [[nodiscard]] auto image_usage() const -> VkImageUsageFlags {
    switch (usage) {
      case ImageUsage::Color:
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      case ImageUsage::Depth:
        return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    TR_ASSERT(false, "unexpected");
  }
  [[nodiscard]] auto aspect_mask() const -> VkImageAspectFlags {
    switch (usage) {
      case ImageUsage::Color:
        return VK_IMAGE_ASPECT_COLOR_BIT;
      case ImageUsage::Depth:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    TR_ASSERT(false, "unexpected");
  }
};

struct ImageBuilder {
  VkDevice device;
  VmaAllocator allocator;
  Swapchain* swapchain;

  [[nodiscard]] auto build_image(ImageDefinition definition, std::string_view debug_name) const -> ImageRessource {
    const auto format = definition.format(*swapchain);
    const auto aspect_mask = definition.aspect_mask();

    VkImageCreateInfo image_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent =
            VkExtent3D{
                .width = swapchain->extent.width,
                .height = swapchain->extent.height,
                .depth = 1,
            },
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
};

// Storage for ressources that are used based on frame_id
struct ImageRessourceStorage {
  ImageDefinition definition;
  std::array<ImageRessource, MAX_FRAMES_IN_FLIGHT> ressources;
  std::string_view debug_name;

  auto store(uint32_t frame_id, ImageRessource ressource) { ressources[frame_id % MAX_FRAMES_IN_FLIGHT] = ressource; }
  auto get(uint32_t frame_id) -> ImageRessource { return ressources[frame_id % MAX_FRAMES_IN_FLIGHT]; }

  void init(ImageBuilder& rb) {
    for (auto& res : ressources) {
      res = rb.build_image(definition, debug_name);
    }
  }

  void defer_deletion(VmaDeletionStack& vma_deletion_stack, DeviceDeletionStack& devide_deletion_stack) {
    for (auto& res : ressources) {
      devide_deletion_stack.defer_deletion(DeviceHandle::ImageView, res.view);
      vma_deletion_stack.defer_deletion(VmaHandle::Image, res.image, res.alloc);
    }
  }
};

}  // namespace tr::renderer
