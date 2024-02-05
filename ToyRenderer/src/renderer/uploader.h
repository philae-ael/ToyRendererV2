#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

#include "buffer.h"
#include "deletion_stack.h"
#include "utils/assert.h"
#include "utils/cast.h"
#include "utils/misc.h"

namespace tr {
namespace renderer {
struct ImageRessource;
}  // namespace renderer
}  // namespace tr

namespace tr::renderer {
struct StagingBuffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation alloc = nullptr;
  VmaAllocationInfo alloc_info{};

  uint32_t offset = 0;
  uint32_t to_upload = 0;

  static auto init(VmaAllocator allocator, std::uint32_t size = 65536) -> StagingBuffer;
  void defer_deletion(VmaDeletionStack& vma_deletion_stack) const {
    vma_deletion_stack.defer_deletion(VmaHandle::Buffer, buffer, alloc);
  }

  [[nodiscard]] auto available(std::size_t alignement = 1) const -> std::size_t {
    return alloc_info.size - utils::align(offset, utils::narrow_cast<uint32_t>(alignement));
  }
  auto consume(std::size_t size, std::size_t alignement = 1) -> std::span<std::byte>;
  auto commit(VkCommandBuffer, VkBuffer, uint32_t offset) -> StagingBuffer&;
  auto commit_image(VkCommandBuffer cmd, const ImageRessource& image, VkRect2D r) -> StagingBuffer&;
  void reset();
};

struct MappedMemoryRange {
  std::span<std::byte> mapped;
};

class Uploader {
 public:
  Uploader(Uploader&& other) noexcept
      : allocator(std::exchange(other.allocator, nullptr)), staging_buffers(std::exchange(other.staging_buffers, {})) {}
  static auto init(VmaAllocator allocator) -> Uploader;

  auto map(std::size_t size, std::size_t alignement = 1) -> MappedMemoryRange;
  void commit_buffer(VkCommandBuffer cmd, MappedMemoryRange mapped, VkBuffer buf, std::size_t size, std::size_t offset);
  void commit_image(VkCommandBuffer cmd, MappedMemoryRange mapped, const ImageRessource& image, VkRect2D r,
                    std::size_t size);

  auto upload_buffer(VkCommandBuffer cmd, VkBuffer dst, std::size_t offset, std::span<const std::byte> src,
                     std::size_t alignemnt = 1) {
    auto data = map(src.size(), alignemnt);
    TR_ASSERT(src.size() == data.mapped.size(), "Can't upload buffer all at once");

    std::memcpy(data.mapped.data(), src.data(), src.size());
    commit_buffer(cmd, data, dst, src.size(), offset);
  }

  // TODO: support memory layouts + stride and other things, maybe
  auto upload_image(VkCommandBuffer cmd, const ImageRessource& image, VkRect2D r, std::span<const std::byte> src,
                    std::size_t alignemnt = 1) {
    auto data = map(src.size(), alignemnt);
    TR_ASSERT(src.size() == data.mapped.size(), "Can't upload image all at once");

    std::memcpy(data.mapped.data(), src.data(), src.size());
    commit_image(cmd, data, image, r, src.size());
  }

  // Reset memory for staging_buffers
  void defer_trim(VmaDeletionStack& allocator_deletion_queue);

  ~Uploader() { TR_ASSERT(staging_buffers.empty(), "Trim Uploader before deleting it"); }

  Uploader(const Uploader&) = delete;
  auto operator=(const Uploader&) -> Uploader& = delete;
  auto operator=(Uploader&& other) -> Uploader& = delete;

 private:
  explicit Uploader(VmaAllocator allocator_) : allocator(allocator_) {}

  VmaAllocator allocator;
  std::vector<tr::renderer::StagingBuffer> staging_buffers{};
  std::size_t staging_buffer_size = 1 << 25;
};

struct Transferer {
  OneTimeCommandBuffer cmd;
  OneTimeCommandBuffer graphics_cmd;
  uint32_t transfer_queue_family;
  uint32_t graphics_queue_family;
  Uploader uploader;

  void upload_buffer(VkBuffer dst, std::size_t offset, std::span<const std::byte> src, std::size_t alignement = 1) {
    uploader.upload_buffer(cmd.vk_cmd, dst, offset, src, alignement);
  }
  void upload_image(const ImageRessource& image, VkRect2D r, std::span<const std::byte> src,
                    std::size_t alignemnt = 1) {
    uploader.upload_image(cmd.vk_cmd, image, r, src, alignemnt);
  }
};

}  // namespace tr::renderer
