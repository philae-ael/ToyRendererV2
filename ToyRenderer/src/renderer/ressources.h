#pragma once
#include <utils/assert.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "constants.h"
#include "deletion_queue.h"
#include "swapchain.h"
#include "synchronisation.h"

namespace tr::renderer {

struct BufferRessource {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation alloc = nullptr;

  VkBufferUsageFlags usage = 0;
  uint32_t size = 0;

  void defer_deletion(VmaDeletionStack& vma_deletion_stack) const {
    vma_deletion_stack.defer_deletion(VmaHandle::Buffer, buffer, alloc);
  }
};

enum BufferOptionFlagsBits {
  BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT = 1 << 1,
};

using BufferOptionFlags = std::uint32_t;

struct BufferDefinition {
  VkBufferUsageFlags usage;
  uint32_t size;
  BufferOptionFlags flags;

  [[nodiscard]] auto vma_required_flags() const -> VkMemoryPropertyFlags;
  [[nodiscard]] auto vma_prefered_flags() const -> VkMemoryPropertyFlags;
  [[nodiscard]] auto vma_usage() const -> VmaMemoryUsage;
  [[nodiscard]] auto vma_flags() const -> VmaAllocationCreateFlags;
};

class BufferBuilder {
 public:
  BufferBuilder(VkDevice device, VmaAllocator allocator) : device(device), allocator(allocator) {}
  [[nodiscard]] auto build_buffer(BufferDefinition definition, std::string_view debug_name) const -> BufferRessource;

 private:
  VkDevice device;
  VmaAllocator allocator;
};

enum ImageUsageBits {
  IMAGE_USAGE_COLOR_BIT = 1 << 1,
  IMAGE_USAGE_DEPTH_BIT = 1 << 2,
  IMAGE_USAGE_SAMPLED_BIT = 1 << 3,
  IMAGE_USAGE_TRANSFER_DST_BIT = 1 << 4,
};

using ImageUsage = std::uint32_t;

struct ImageRessource {
  VkImage image;
  VkImageView view;
  SyncInfo sync_info;
  VmaAllocation alloc;
  ImageUsage usage;

  static auto from_external_image(VkImage image, VkImageView view, ImageUsage usage,
                                  SyncInfo sync_info = SrcImageMemoryBarrierUndefined) -> ImageRessource;
  auto invalidate() -> ImageRessource&;
  [[nodiscard]] auto prepare_barrier(SyncInfo dst) -> std::optional<VkImageMemoryBarrier2>;

  auto as_attachment(std::optional<VkClearValue> clearValue) -> VkRenderingAttachmentInfo;

  void defer_deletion(VmaDeletionStack& vma_deletion_stack, DeviceDeletionStack& device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::ImageView, view);
    vma_deletion_stack.defer_deletion(VmaHandle::Image, image, alloc);
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
  IMAGE_OPTION_FLAG_SIZE_CUSTOM_BIT = 1 << 1,
  IMAGE_OPTION_FLAG_FORMAT_R8G8B8A8_UNORM_BIT = 1 << 2,  // VK_FORMAT_R8G8B8A8_UINT
  IMAGE_OPTION_FLAG_FORMAT_R32G32B32A32_SFLOAT_BIT = 1 << 3,
  IMAGE_OPTION_FLAG_FORMAT_D16_UNORM_BIT = 1 << 4,  // VK_FORMAT_D32_SFLOAT
  IMAGE_OPTION_FLAG_FORMAT_SAME_AS_FRAMEBUFFER_BIT = 1 << 5,
  IMAGE_OPTION_FLAG_COLOR_ATTACHMENT_BIT = 1 << 6,
  IMAGE_OPTION_FLAG_TEXTURE_ATTACHMENT_BIT = 1 << 7,
};
using ImageOptionsFlags = std::uint32_t;

struct ImageDefinition {
  ImageOptionsFlags flags;
  ImageUsage usage;
  std::optional<VkExtent2D> size;

  [[nodiscard]] auto format(const Swapchain& swapchain) const -> VkFormat;
  [[nodiscard]] auto image_usage() const -> VkImageUsageFlags;
  [[nodiscard]] auto aspect_mask() const -> VkImageAspectFlags;
  [[nodiscard]] auto extent(const Swapchain& swapchain) const -> VkExtent3D;
};

class ImageBuilder {
 public:
  ImageBuilder(VkDevice device, VmaAllocator allocator, Swapchain* swapchain)
      : device(device), allocator(allocator), swapchain(swapchain) {}

  [[nodiscard]] auto build_image(ImageDefinition definition, std::string_view debug_name) const -> ImageRessource;

 private:
  VkDevice device;
  VmaAllocator allocator;
  const Swapchain* swapchain;
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

  void defer_deletion(VmaDeletionStack& vma_deletion_stack, DeviceDeletionStack& device_deletion_stack) {
    for (auto& res : ressources) {
      res.defer_deletion(vma_deletion_stack, device_deletion_stack);
    }
  }
};

}  // namespace tr::renderer
