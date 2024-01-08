#include "uploader.h"

#include "vertex.h"

auto tr::renderer::Uploader::init(VmaAllocator allocator) -> Uploader {
  Uploader u{allocator};
  u.staging_buffers.push_back(StagingBuffer::init(allocator, 1 << 28));
  return u;
}

void tr::renderer::Uploader::defer_trim(VmaDeletionStack& allocator_deletion_queue) {
  for (auto& sb : staging_buffers) {
    sb.defer_deletion(allocator_deletion_queue);
  }
  staging_buffers.clear();
}
auto tr::renderer::Uploader::map(std::size_t size_hint) -> MappedMemoryRange {
  return {staging_buffers[0].consume(size_hint)};
}

void tr::renderer::Uploader::commit_buffer(VkCommandBuffer cmd, MappedMemoryRange /*mapped*/, VkBuffer buf,
                                           std::size_t size, std::size_t offset) {
  staging_buffers[0].to_upload = utils::narrow_cast<uint32_t>(size);
  staging_buffers[0].commit(cmd, buf, utils::narrow_cast<uint32_t>(offset));
}

void tr::renderer::Uploader::commit_image(VkCommandBuffer cmd, MappedMemoryRange /*mapped*/,
                                          const ImageRessource& image, VkRect2D r, std::size_t size) {
  staging_buffers[0].to_upload = utils::narrow_cast<uint32_t>(size);
  staging_buffers[0].commit_image(cmd, image, r);
}
