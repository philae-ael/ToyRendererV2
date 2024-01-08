#pragma once
#include <utils/assert.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include "constants.h"
#include "deletion_queue.h"
#include "synchronisation.h"
#include "utils/misc.h"

namespace tr::renderer {

struct Swapchain;

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

struct ImageClearOpLoad {};
struct ImageClearOpDontCare {};

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

  auto as_attachment(std::variant<VkClearValue, ImageClearOpLoad, ImageClearOpDontCare> clearOp)
      -> VkRenderingAttachmentInfo;

  void defer_deletion(VmaDeletionStack& vma_deletion_stack, DeviceDeletionStack& device_deletion_stack) const {
    device_deletion_stack.defer_deletion(DeviceHandle::ImageView, view);
    vma_deletion_stack.defer_deletion(VmaHandle::Image, image, alloc);
  }
};

enum ImageOptionsFlagBits {
  IMAGE_OPTION_FLAG_COLOR_ATTACHMENT_BIT = 1 << 0,
  IMAGE_OPTION_FLAG_TEXTURE_ATTACHMENT_BIT = 1 << 1,
  IMAGE_OPTION_FLAG_EXTERNAL_BIT = 1 << 2,
};
using ImageOptionsFlags = std::uint32_t;

struct FramebufferFormat {};
struct FramebufferExtent {};

struct ImageDefinition {
  ImageOptionsFlags flags;
  ImageUsage usage;
  std::variant<FramebufferExtent, VkExtent2D> size;
  std::variant<FramebufferFormat, VkFormat> format;
  std::string_view debug_name;

  [[nodiscard]] auto vk_format(const Swapchain& swapchain) const -> VkFormat;
  [[nodiscard]] auto vk_image_usage() const -> VkImageUsageFlags;
  [[nodiscard]] auto vk_aspect_mask() const -> VkImageAspectFlags;
  [[nodiscard]] auto vk_extent(const Swapchain& swapchain) const -> VkExtent3D;
  [[nodiscard]] auto depends_on_swapchain() const -> bool {
    return std::holds_alternative<FramebufferExtent>(size) || std::holds_alternative<FramebufferFormat>(format);
  }
};

class ImageBuilder {
 public:
  ImageBuilder(VkDevice device, VmaAllocator allocator, const Swapchain* swapchain)
      : device(device), allocator(allocator), swapchain(swapchain) {}

  [[nodiscard]] auto build_image(ImageDefinition definition) const -> ImageRessource;

 private:
  VkDevice device;
  VmaAllocator allocator;
  const Swapchain* swapchain;
};

enum class ImageRessourceId {
  Swapchain,
  GBuffer0,
  GBuffer1,
  GBuffer2,
  Depth,
  MAX,
};

struct ImageRessourceDefinition {
  ImageDefinition definition;
  ImageRessourceId id{};
};

// Storage for ressources that are used based on frame_id
struct ImageRessourceStorage {
  ImageDefinition definition;
  std::array<ImageRessource, MAX_FRAMES_IN_FLIGHT> ressources{};

  auto store(uint32_t frame_id, ImageRessource ressource) { ressources[frame_id % MAX_FRAMES_IN_FLIGHT] = ressource; }
  auto get(uint32_t frame_id) -> ImageRessource { return ressources[frame_id % MAX_FRAMES_IN_FLIGHT]; }

  void init(ImageBuilder& rb) {
    if ((definition.flags & IMAGE_OPTION_FLAG_EXTERNAL_BIT) != 0) {
      return;
    }
    for (auto& res : ressources) {
      res = rb.build_image(definition);
    }
  }

  void defer_deletion(VmaDeletionStack& vma_deletion_stack, DeviceDeletionStack& device_deletion_stack) {
    for (auto& res : ressources) {
      res.defer_deletion(vma_deletion_stack, device_deletion_stack);
    }

    if ((definition.flags & IMAGE_OPTION_FLAG_EXTERNAL_BIT) != 0) {
      return;
    }

    if (definition.depends_on_swapchain()) {
    } else {
      for (auto& res : ressources) {
        res.defer_deletion(vma_deletion_stack, device_deletion_stack);
      }
    }
  }

  void from_external_images(std::span<VkImage> images, std::span<VkImageView> views) {
    TR_ASSERT(images.size() == views.size(), "??");
    TR_ASSERT(images.size() >= ressources.size(), "??");

    for (size_t i = 0; i < ressources.size(); i++) {
      ressources[i] = ImageRessource::from_external_image(images[i], views[i], definition.vk_image_usage());
    }
  }
};

struct FrameRessourceManager;

class RessourceManager {
  std::array<ImageRessourceStorage, static_cast<size_t>(ImageRessourceId::MAX)> image_storages_;

 public:
  void define_image(ImageRessourceDefinition def) {
    image_storages_[static_cast<size_t>(def.id)].definition = def.definition;
  }

  auto frame(uint32_t frame_index) -> FrameRessourceManager;
  auto get_image_storage(ImageRessourceId id) -> ImageRessourceStorage& {
    return image_storages_[static_cast<size_t>(id)];
  }

  auto image_storages() -> std::span<ImageRessourceStorage> { return image_storages_; }
  friend FrameRessourceManager;
};

struct FrameRessourceManager {
  // TODO: Create a thing like gsl::non_null
  RessourceManager* rm;
  uint32_t frame_index;

  [[nodiscard]] auto get_image(ImageRessourceId id) -> ImageRessource& {
    utils::ignore_unused(this);
    return rm->image_storages_[static_cast<size_t>(id)].ressources[frame_index];
  }
};

}  // namespace tr::renderer
