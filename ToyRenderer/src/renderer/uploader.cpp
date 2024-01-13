#include "uploader.h"

#include <cstddef>

#include "utils/assert.h"
#include "vertex.h"

auto tr::renderer::Uploader::init(VmaAllocator allocator) -> Uploader {
  Uploader u{allocator};
  return u;
}

void tr::renderer::Uploader::defer_trim(VmaDeletionStack& allocator_deletion_queue) {
  for (auto& sb : staging_buffers) {
    sb.defer_deletion(allocator_deletion_queue);
  }
  staging_buffers.clear();
}
auto tr::renderer::Uploader::map(std::size_t size, std::size_t alignement) -> MappedMemoryRange {
  TR_ASSERT(staging_buffer_size > size, "Buffer too big: staging_buffer_size {}, size {}", staging_buffer_size, size);
  if (staging_buffers.empty() || staging_buffers.back().available() < size) {
    staging_buffers.push_back(StagingBuffer::init(allocator, staging_buffer_size));
  }

  return {staging_buffers.back().consume(size, alignement)};
}

void tr::renderer::Uploader::commit_buffer(VkCommandBuffer cmd, MappedMemoryRange /*mapped*/, VkBuffer buf,
                                           std::size_t size, std::size_t offset) {
  staging_buffers.back().to_upload = utils::narrow_cast<uint32_t>(size);
  staging_buffers.back().commit(cmd, buf, utils::narrow_cast<uint32_t>(offset));
}

void tr::renderer::Uploader::commit_image(VkCommandBuffer cmd, MappedMemoryRange /*mapped*/,
                                          const ImageRessource& image, VkRect2D r, std::size_t size) {
  staging_buffers.back().to_upload = utils::narrow_cast<uint32_t>(size);
  staging_buffers.back().commit_image(cmd, image, r);
}
