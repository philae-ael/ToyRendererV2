#pragma once
#include <utils/assert.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <bit>
#include <cstdint>
#include <optional>

#include "constants.h"
#include "deletion_queue.h"
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
  SrcImageMemoryBarrier src;
  VmaAllocation alloc;
  ImageUsage usage;

  auto transition(VkCommandBuffer cmd, DstImageMemoryBarrier dst) -> ImageRessource& {
    if (dst.newLayout != src.oldLayout || dst.dstQueueFamilyIndex != src.srcQueueFamilyIndex) {
      VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;
      switch (usage) {
        case ImageUsage::Color:
          aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          break;
        case ImageUsage::Depth:
          aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
          break;
      }
      src.merge(dst, image,
                VkImageSubresourceRange{
                    .aspectMask = aspectMask,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                })
          .submit(cmd);
    }
    src = SrcImageMemoryBarrier{
        .srcAccessMask = dst.dstAccessMask,
        .oldLayout = dst.newLayout,
        .srcQueueFamilyIndex = dst.dstQueueFamilyIndex,
        .srcStageMask = dst.dstStageMask,
    };
    return *this;
  }

  auto as_attachment(std::optional<VkClearValue> clearValue) -> VkRenderingAttachmentInfo {
    return VkRenderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = view,
        .imageLayout = src.oldLayout,
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
  IMAGE_OPTION_FLAG_FORMAT_B8G8R8A8_UINT_BIT = 1 << 1,  // VK_FORMAT_R8G8B8A8_UINT
  IMAGE_OPTION_FLAG_FORMAT_R32G32B32A32_SFLOAT_BIT = 1 << 2,
  IMAGE_OPTION_FLAG_FORMAT_D32_BIT = 1 << 3,  // VK_FORMAT_D32_SFLOAT
  IMAGE_OPTION_FLAG_COLOR_ATTACHMENT_BIT = 1 << 4,
  IMAGE_OPTION_FLAG_TEXTURE_ATTACHMENT_BIT = 1 << 5,
};
using ImageOptionsFlags = std::uint32_t;

struct ImageDefinition {
  ImageOptionsFlags flags;
  ImageUsage usage;
};

struct ImageBuilder {
  VkDevice device;
  VmaAllocator allocator;
  VkExtent2D extent;

  [[nodiscard]] auto build_image(ImageDefinition definition) const -> ImageRessource {
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageCreateFlags create_flags = 0;
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;

    TR_ASSERT(std::popcount((definition.flags & (IMAGE_OPTION_FLAG_FORMAT_B8G8R8A8_UINT_BIT |
                                                 IMAGE_OPTION_FLAG_FORMAT_R32G32B32A32_SFLOAT_BIT |
                                                 IMAGE_OPTION_FLAG_FORMAT_D32_BIT))) == 1,
              "exacly one format has to be specified");

    if ((definition.flags & IMAGE_OPTION_FLAG_FORMAT_B8G8R8A8_UINT_BIT) != 0) {
      TR_ASSERT(definition.usage == ImageUsage::Color, "BAD USAGE");
      format = VK_FORMAT_B8G8R8A8_UNORM;
    } else if ((definition.flags & IMAGE_OPTION_FLAG_FORMAT_R32G32B32A32_SFLOAT_BIT) != 0) {
      format = VK_FORMAT_R32G32B32_SFLOAT;
    } else if ((definition.flags & IMAGE_OPTION_FLAG_FORMAT_D32_BIT) != 0) {
      format = VK_FORMAT_D32_SFLOAT;
    }

    switch (definition.usage) {
      case ImageUsage::Color:
        usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        break;
      case ImageUsage::Depth:
        usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        break;
    }

    VkImageCreateInfo image_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = create_flags,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent =
            VkExtent3D{
                .width = extent.width,
                .height = extent.height,
                .depth = 1,
            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = samples,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
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
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    VkImageView view = VK_NULL_HANDLE;
    VK_UNWRAP(vkCreateImageView, device, &view_create_info, nullptr, &view);

    return ImageRessource{
        .image = image,
        .view = view,
        .src = SrcImageMemoryBarrierUndefined,
        .alloc = alloc,
        .usage = definition.usage,
    };
  }
};

// Storage for ressources that are used based on frame_id
struct ImageRessourceStorage {
  ImageDefinition definition;
  std::array<ImageRessource, MAX_FRAMES_IN_FLIGHT> ressources;

  auto store(uint32_t frame_id, ImageRessource ressource) { ressources[frame_id % MAX_FRAMES_IN_FLIGHT] = ressource; }
  auto get(uint32_t frame_id) -> ImageRessource { return ressources[frame_id % MAX_FRAMES_IN_FLIGHT]; }

  void init(ImageBuilder& rb) {
    for (auto& res : ressources) {
      res = rb.build_image(definition);
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
