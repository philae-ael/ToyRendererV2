#pragma once
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include "../registry.h"
#include "deletion_stack.h"
#include "synchronisation.h"

namespace tr::renderer {

struct Swapchain;

enum class RessourceScope : uint8_t {
  Invalid,
  Transient,
  Extern,
  Storage,
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
  AO,
  MAX,
};

enum class BufferRessourceId {
  Camera,
  ShadowCamera,
  DebugVertices,
  MAX,
};

struct BufferRessource {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation alloc = nullptr;
  void* mapped_data = nullptr;

  VkBufferUsageFlags usage = 0;
  uint32_t size = 0;

  void tie(Lifetime& lifetime) const { lifetime.tie(VmaHandle::Buffer, buffer, alloc); }
};

using BufferOptionFlags = std::uint32_t;
enum BufferOptionFlagsBits {
  BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT = 1 << 0,
  BUFFER_OPTION_FLAG_CREATE_MAPPED_BIT = 1 << 1,
};

struct BufferDefinition {
  VkBufferUsageFlags usage;
  uint32_t size;
  BufferOptionFlags flags;
  std::string_view debug_name;

  [[nodiscard]] auto vma_required_flags() const -> VkMemoryPropertyFlags;
  [[nodiscard]] auto vma_prefered_flags() const -> VkMemoryPropertyFlags;
  [[nodiscard]] auto vma_usage() const -> VmaMemoryUsage;
  [[nodiscard]] auto vma_flags() const -> VmaAllocationCreateFlags;

  friend auto operator<=>(const BufferDefinition& a, const BufferDefinition& b) = default;
};

class BufferBuilder {
 public:
  BufferBuilder(VkDevice device_, VmaAllocator allocator_) : device(device_), allocator(allocator_) {}
  [[nodiscard]] auto build_buffer(BufferDefinition definition) const -> BufferRessource;

 private:
  VkDevice device;
  VmaAllocator allocator;
};

struct ImageClearOpLoad {};
struct ImageClearOpDontCare {};
using ClearOp = std::variant<VkClearValue, ImageClearOpLoad, ImageClearOpDontCare>;

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

  auto as_attachment(ClearOp clearOp) -> VkRenderingAttachmentInfo;

  void tie(Lifetime& lifetime) const {
    lifetime.tie(VmaHandle::Image, image, alloc);
    lifetime.tie(DeviceHandle::ImageView, view);
  }
};

using ImageOptionsFlags = std::uint32_t;
enum ImageOptionsFlagBits {
  IMAGE_OPTION_FLAG_COLOR_ATTACHMENT_BIT = 1 << 0,
  IMAGE_OPTION_FLAG_TEXTURE_ATTACHMENT_BIT = 1 << 1,
};

enum class ImageDependency {
  Swapchain,
  CVAR,
};

struct StaticExtent {
  uint32_t w, h;

  constexpr friend auto operator<=>(const StaticExtent&, const StaticExtent&) = default;
  [[nodiscard]] static auto depends_on(ImageDependency /*dep*/) -> bool { return false; }
  [[nodiscard]] auto resolve(const Swapchain& /*swapchain*/) const -> VkExtent3D { return {w, h, 1}; }
};

struct CVarExtent {
  CVarExtent2D cvar;
  constexpr friend auto operator<=>(const CVarExtent&, const CVarExtent&) = default;
  [[nodiscard]] static auto depends_on(ImageDependency dep) -> bool {
    switch (dep) {
      case ImageDependency::Swapchain:
        return false;
      case ImageDependency::CVAR:
        return true;
    }
  }

  [[nodiscard]] auto resolve(const Swapchain& /*swapchain*/) const -> VkExtent3D {
    VkExtent2D ext = cvar.resolve();
    return {ext.width, ext.height, 1};
  }
};
struct InternalResolutionExtent {
  constexpr friend auto operator<=>(const InternalResolutionExtent&, const InternalResolutionExtent&) = default;
  [[nodiscard]] static auto depends_on(ImageDependency dep) -> bool {
    switch (dep) {
      case ImageDependency::Swapchain:
      case ImageDependency::CVAR:
        return true;
    }
  }

  [[nodiscard]] static auto resolve(const Swapchain& /*swapchain*/) -> VkExtent3D;
};

struct SwapchainExtent {
  constexpr friend auto operator<=>(const SwapchainExtent&, const SwapchainExtent&) = default;
  [[nodiscard]] static auto depends_on(ImageDependency dep) -> bool {
    switch (dep) {
      case ImageDependency::Swapchain:
        return true;
      case ImageDependency::CVAR:
        return false;
    }
  }
  static auto resolve(const Swapchain& swapchain) -> VkExtent3D;
};

struct ImageExtent : std::variant<SwapchainExtent, StaticExtent, InternalResolutionExtent, CVarExtent> {
  constexpr friend auto operator<=>(const ImageExtent& a, const ImageExtent& b) = default;
  [[nodiscard]] auto depends_on(ImageDependency dep) const -> bool;
  [[nodiscard]] auto resolve(const Swapchain& swapchain) const -> VkExtent3D;
};

struct SwapchainFormat {
  [[nodiscard]] static auto depends_on(ImageDependency dep) -> bool {
    switch (dep) {
      case ImageDependency::Swapchain:
        return true;
      case ImageDependency::CVAR:
        return false;
    }
  }
  constexpr friend auto operator<=>(const SwapchainFormat& /*unused*/, const SwapchainFormat& /*unused*/) = default;
  [[nodiscard]] static auto resolve(const Swapchain& swapchain) -> VkFormat;
};

struct StaticFormat {
  VkFormat format;
  [[nodiscard]] static auto depends_on(ImageDependency /*dep*/) -> bool { return false; }
  constexpr friend auto operator<=>(const StaticFormat& /*unused*/, const StaticFormat& /*unused*/) = default;
  [[nodiscard]] auto resolve(const Swapchain& /*swapchain*/) const -> VkFormat { return format; }
};

struct ImageFormat : std::variant<SwapchainFormat, StaticFormat> {
  constexpr friend auto operator<=>(const ImageFormat& a, const ImageFormat& b) = default;
  [[nodiscard]] auto resolve(const Swapchain& swapchain) const -> VkFormat;
  [[nodiscard]] auto depends_on(ImageDependency dep) const -> bool;
};

struct ImageDefinition {
  ImageOptionsFlags flags;
  VkImageUsageFlags usage;
  ImageExtent size;
  ImageFormat format;
  std::string_view debug_name;

  [[nodiscard]] auto vk_format(const Swapchain& swapchain) const -> VkFormat;
  [[nodiscard]] auto vk_aspect_mask() const -> VkImageAspectFlags;
  [[nodiscard]] auto vk_extent(const Swapchain& swapchain) const -> VkExtent3D;
  [[nodiscard]] auto depends_on(ImageDependency dep) const -> bool {
    return size.depends_on(dep) || format.depends_on(dep);
  }

  friend auto operator<=>(const ImageDefinition& a, const ImageDefinition& b) = default;
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

struct ImageRessourceDefinition {
  ImageRessourceId id{};
  ImageDefinition definition;
  RessourceScope scope{};
};

struct BufferRessourceDefinition {
  BufferRessourceId id{};
  BufferDefinition definition;
  RessourceScope scope{};
};
}  // namespace tr::renderer
