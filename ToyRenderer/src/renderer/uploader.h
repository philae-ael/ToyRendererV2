#pragma once

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

#include "deletion_queue.h"
#include "ressources.h"
#include "swapchain.h"
#include "utils/assert.h"
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
  void commit_buffer(VkCommandBuffer cmd, MappedMemoryRange mapped, VkBuffer buf, std::size_t size, std::size_t offset);
  void commit_image(VkCommandBuffer cmd, MappedMemoryRange mapped, const ImageRessource& image, VkRect2D r,
                    std::size_t size);

  auto upload_buffer(VkCommandBuffer cmd, VkBuffer dst, std::size_t offset, std::span<const std::byte> src) {
    auto src_still_to_upload = src;
    while (!src_still_to_upload.empty()) {
      auto data = map(src_still_to_upload.size());

      auto to_copy = std::min(data.mapped.size(), src_still_to_upload.size());
      std::memcpy(data.mapped.data(), src_still_to_upload.data(), to_copy);

      commit_buffer(cmd, data, dst, to_copy, offset);

      src_still_to_upload = src_still_to_upload.subspan(to_copy);
      offset += to_copy;
    }
  }

  // TODO: support memory layouts + stride and other things, maybe
  auto upload_image(VkCommandBuffer cmd, const ImageRessource& image, VkRect2D r, std::span<const std::byte> src) {
    auto data = map(src.size());
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
  explicit Uploader(VmaAllocator allocator) : allocator(allocator) {}

  VmaAllocator allocator;
  std::vector<tr::renderer::StagingBuffer> staging_buffers{};
};

struct Transferer {
  OneTimeCommandBuffer cmd;
  Uploader uploader;

  void upload_buffer(VkBuffer dst, std::size_t offset, std::span<const std::byte> src) {
    uploader.upload_buffer(cmd.vk_cmd, dst, offset, src);
  }
  void upload_image(const ImageRessource& image, VkRect2D r, std::span<const std::byte> src) {
    uploader.upload_image(cmd.vk_cmd, image, r, src);
  }
};

}  // namespace tr::renderer
