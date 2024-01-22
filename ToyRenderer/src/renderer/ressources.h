#pragma once
#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <utils/misc.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include "../registry.h"
#include "constants.h"
#include "deletion_stack.h"
#include "synchronisation.h"

namespace tr::renderer {

struct Swapchain;

struct BufferRessource {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation alloc = nullptr;

  VkBufferUsageFlags usage = 0;
  uint32_t size = 0;
};

enum BufferOptionFlagsBits {
  BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT = 1 << 0,
};

using BufferOptionFlags = std::uint32_t;

enum class BufferRessourceId { Camera, ShadowCamera, MAX };

struct BufferDefinition {
  VkBufferUsageFlags usage;
  uint32_t size;
  BufferOptionFlags flags;
  std::string_view debug_name;

  [[nodiscard]] auto vma_required_flags() const -> VkMemoryPropertyFlags;
  [[nodiscard]] auto vma_prefered_flags() const -> VkMemoryPropertyFlags;
  [[nodiscard]] auto vma_usage() const -> VmaMemoryUsage;
  [[nodiscard]] auto vma_flags() const -> VmaAllocationCreateFlags;
};

class BufferBuilder {
 public:
  BufferBuilder(VkDevice device_, VmaAllocator allocator_) : device(device_), allocator(allocator_) {}
  [[nodiscard]] auto build_buffer(Lifetime& lifetime, BufferDefinition definition) const -> BufferRessource;

 private:
  VkDevice device;
  VmaAllocator allocator;
};

struct ImageClearOpLoad {};
struct ImageClearOpDontCare {};

struct ImageRessource {
  VkImage image;
  VkImageView view;
  SyncInfo sync_info;
  VmaAllocation alloc;
  VkImageUsageFlags usage;
  VkExtent2D extent;

  static auto from_external_image(VkImage image, VkImageView view, VkImageUsageFlags usage, VkExtent2D extent,
                                  SyncInfo sync_info = SrcImageMemoryBarrierUndefined) -> ImageRessource;
  auto invalidate() -> ImageRessource&;
  [[nodiscard]] auto prepare_barrier(SyncInfo dst) -> std::optional<VkImageMemoryBarrier2>;

  auto as_attachment(std::variant<VkClearValue, ImageClearOpLoad, ImageClearOpDontCare> clearOp)
      -> VkRenderingAttachmentInfo;

  void tie(Lifetime& lifetime) const {
    lifetime.tie(VmaHandle::Image, image, alloc);
    lifetime.tie(DeviceHandle::ImageView, view);
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
struct InternalResolution {};

struct CVarExtent {
  const char* name;
  VkExtent2D default_;

  [[nodiscard]] auto resolve() const -> VkExtent2D {
    const auto vwidth = tr::Registry::global()["cvar"][name]["x"];
    const auto vheight = tr::Registry::global()["cvar"][name]["y"];
    if (!vheight.isUInt() || !vheight.isUInt()) {
      save(default_);
      return default_;
    }

    const auto width = vwidth.asUInt();
    const auto height = vheight.asUInt();
    return VkExtent2D{width, height};
  }

  void save(VkExtent2D extent) const {
    tr::Registry::global()["cvar"][name]["x"] = extent.width;
    tr::Registry::global()["cvar"][name]["y"] = extent.height;
  }
};

struct ImageDefinition {
  ImageOptionsFlags flags;
  VkImageUsageFlags usage;
  std::variant<FramebufferExtent, InternalResolution, VkExtent2D, CVarExtent> size;
  std::variant<FramebufferFormat, VkFormat> format;
  std::string_view debug_name;

  [[nodiscard]] auto vk_format(const Swapchain& swapchain) const -> VkFormat;
  [[nodiscard]] auto vk_aspect_mask() const -> VkImageAspectFlags;
  [[nodiscard]] auto vk_extent(const Swapchain& swapchain) const -> VkExtent3D;
  [[nodiscard]] auto depends_on_swapchain() const -> bool {
    return std::holds_alternative<FramebufferExtent>(size) || std::holds_alternative<InternalResolution>(size) ||
           std::holds_alternative<FramebufferFormat>(format);
  }
};

class ImageBuilder {
 public:
  ImageBuilder(VkDevice device_, VmaAllocator allocator_, const Swapchain* swapchain_)
      : device(device_), allocator(allocator_), swapchain(swapchain_) {}

  [[nodiscard]] auto build_image(ImageDefinition definition) const -> ImageRessource;

 private:
  VkDevice device;
  VmaAllocator allocator;
  const Swapchain* swapchain;
};

enum class ImageRessourceId {
  Swapchain,
  Rendered,
  GBuffer0,
  GBuffer1,
  GBuffer2,
  GBuffer3,
  Depth,
  ShadowMap,
  MAX,
};

struct ImageRessourceDefinition {
  ImageDefinition definition;
  ImageRessourceId id{};
};

struct BufferRessourceDefinition {
  BufferDefinition definition;
  BufferRessourceId id{};
};

// Storage for ressources that are used based on frame_id
struct ImageRessourceStorage {
  ImageDefinition definition;
  std::array<ImageRessource, MAX_FRAMES_IN_FLIGHT> ressources{};
  bool invalidated = false;

  auto store(uint32_t frame_id, ImageRessource ressource) { ressources[frame_id % MAX_FRAMES_IN_FLIGHT] = ressource; }
  auto get(uint32_t frame_id) -> ImageRessource& {
    if (invalidated) {
      spdlog::warn("invalidated image ressource is used!");
    }
    return ressources[frame_id % MAX_FRAMES_IN_FLIGHT];
  }

  void init(ImageBuilder& rb) {
    if ((definition.flags & IMAGE_OPTION_FLAG_EXTERNAL_BIT) != 0) {
      return;
    }
    for (auto& res : ressources) {
      res = rb.build_image(definition);
    }
    invalidated = false;
  }

  void tie(Lifetime& lifetime) const {
    if ((definition.flags & IMAGE_OPTION_FLAG_EXTERNAL_BIT) != 0) {
      return;
    }
    for (const auto& res : ressources) {
      res.tie(lifetime);
    }
  }

  void from_external_images(std::span<VkImage> images, std::span<VkImageView> views, VkExtent2D extent) {
    TR_ASSERT(images.size() == views.size(), "??");
    TR_ASSERT(images.size() >= ressources.size(), "??");

    for (size_t i = 0; i < ressources.size(); i++) {
      ressources[i] = ImageRessource::from_external_image(images[i], views[i], definition.usage, extent);
    }
  }
};

struct BufferRessourceStorage {
  BufferDefinition definition;
  std::array<BufferRessource, MAX_FRAMES_IN_FLIGHT> ressources{};

  auto store(uint32_t frame_id, BufferRessource ressource) { ressources[frame_id % MAX_FRAMES_IN_FLIGHT] = ressource; }
  auto get(uint32_t frame_id) -> BufferRessource& { return ressources[frame_id % MAX_FRAMES_IN_FLIGHT]; }

  void init(Lifetime& lifetime, BufferBuilder& bb) {
    for (auto& res : ressources) {
      res = bb.build_buffer(lifetime, definition);
    }
  }
};

struct FrameRessourceManager;

class RessourceManager {
  std::array<ImageRessourceStorage, static_cast<size_t>(ImageRessourceId::MAX)> image_storages_;
  std::array<BufferRessourceStorage, static_cast<size_t>(BufferRessourceId::MAX)> buffer_storages_;

 public:
  bool has_invalidated = false;

  void define_image(ImageRessourceDefinition def) {
    image_storages_[static_cast<size_t>(def.id)].definition = def.definition;
  }
  [[nodiscard]] auto image_definition(ImageRessourceId id) const -> ImageDefinition {
    return image_storages_[static_cast<size_t>(id)].definition;
  }
  void define_buffer(BufferRessourceDefinition def) {
    buffer_storages_[static_cast<size_t>(def.id)].definition = def.definition;
  }
  [[nodiscard]] auto buffer_definition(BufferRessourceId id) const -> BufferDefinition {
    return buffer_storages_[static_cast<size_t>(id)].definition;
  }

  auto frame(uint32_t frame_index) -> FrameRessourceManager;
  auto get_image_storage(ImageRessourceId id) -> ImageRessourceStorage& {
    return image_storages_[static_cast<size_t>(id)];
  }

  auto image_storages() -> std::span<ImageRessourceStorage> { return image_storages_; }
  auto buffer_storages() -> std::span<BufferRessourceStorage> { return buffer_storages_; }

  auto invalidate_image(ImageRessourceId id) {
    get_image_storage(id).invalidated = true;
    has_invalidated = true;
  }
};

struct FrameRessourceManager {
  utils::types::not_null_pointer<RessourceManager> rm;
  uint32_t frame_index;

  [[nodiscard]] auto get_image(ImageRessourceId id) -> ImageRessource& {
    return rm->image_storages()[static_cast<size_t>(id)].get(frame_index);
  }

  [[nodiscard]] auto get_buffer(BufferRessourceId id) -> BufferRessource& {
    return rm->buffer_storages()[static_cast<size_t>(id)].get(frame_index);
  }

  template <class T, class Fn>
  void update_buffer(VmaAllocator allocator, BufferRessourceId id, Fn&& f) {
    T* map = nullptr;
    const auto& buf = get_buffer(id);
    vmaMapMemory(allocator, buf.alloc, reinterpret_cast<void**>(&map));

    std::forward<Fn>(f)(map);
    vmaUnmapMemory(allocator, buf.alloc);
  }
};

}  // namespace tr::renderer
