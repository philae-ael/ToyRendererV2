#pragma once

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

#include "deletion_queue.h"
#include "vertex.h"
namespace tr::renderer {

struct MappedMemoryRange {
  std::span<std::byte> mapped;
};

class Uploader {
 public:
  Uploader(Uploader&& other) noexcept
      : allocator(std::exchange(other.allocator, nullptr)), staging_buffers(std::exchange(other.staging_buffers, {})) {}
  static auto init(VmaAllocator allocator) -> Uploader;

  auto map(std::size_t size_hint = 4096) -> MappedMemoryRange;
  void commit(VkCommandBuffer cmd, MappedMemoryRange mapped, VkBuffer buf, std::size_t size, std::size_t offset);

  auto upload(VkCommandBuffer cmd, VkBuffer dst, std::size_t offset, std::span<const std::byte> src) {
    auto src_still_to_upload = src;
    while (!src_still_to_upload.empty()) {
      auto data = map(src_still_to_upload.size());

      auto to_copy = std::min(data.mapped.size(), src_still_to_upload.size());
      std::memcpy(data.mapped.data(), src_still_to_upload.data(), to_copy);

      commit(cmd, data, dst, to_copy, offset);

      src_still_to_upload = src_still_to_upload.subspan(to_copy);
      offset += to_copy;
    }
  }

  // Reset memory for staging_buffers
  void defer_trim(VmaDeletionStack& allocator_deletion_queue);

  ~Uploader() { TR_ASSERT(staging_buffers.empty(), "Trim Uploader before deleting it"); }

  Uploader(const Uploader&) = delete;
  auto operator=(const Uploader&) -> Uploader& = delete;
  auto operator=(Uploader&& other) -> Uploader& = delete;

 private:
  explicit Uploader(VmaAllocator allocator) : allocator(allocator) {}

  VmaAllocator allocator;
  std::vector<tr::renderer::StagingBuffer> staging_buffers{};
};

}  // namespace tr::renderer
