#ifndef RESSOURCE_MANAGER_H
#define RESSOURCE_MANAGER_H

#include <vulkan/vulkan_core.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ressources.h"
#include "utils/assert.h"
#include "utils/cast.h"

namespace tr::renderer {

enum class image_ressource_handle : uint32_t {};
enum class buffer_ressource_handle : uint32_t {};

struct ImageRessourceInfo {
  uint16_t index;
  RessourceScope scope;

  [[nodiscard]] auto into_handle() const -> image_ressource_handle {
    return std::bit_cast<image_ressource_handle>(*this);
  }
  static auto from_handle(image_ressource_handle handle) -> ImageRessourceInfo {
    return std::bit_cast<ImageRessourceInfo>(handle);
  }
};

struct BufferRessourceInfo {
  uint16_t index;
  RessourceScope scope;

  [[nodiscard]] auto into_handle() const -> buffer_ressource_handle {
    return std::bit_cast<buffer_ressource_handle>(*this);
  }
  static auto from_handle(buffer_ressource_handle handle) -> BufferRessourceInfo {
    return std::bit_cast<BufferRessourceInfo>(handle);
  }
};

struct FrameRessourceData {
  std::vector<VkDescriptorImageInfo> descriptor_image_infos;
  std::vector<ImageRessource> image_ressource;
  std::size_t transient_images_offset{};
  std::size_t storage_images_offset{};
  std::size_t external_images_offset{};

  std::vector<BufferRessource> buffer_ressource;
  std::size_t transient_buffers_offset{};
  std::size_t storage_buffers_offset{};
  std::size_t external_buffers_offset{};

  [[nodiscard]] auto image_index(image_ressource_handle handle) const -> uint32_t {
    const auto info = ImageRessourceInfo::from_handle(handle);
    switch (info.scope) {
      case RessourceScope::Transient:
        return utils::narrow_cast<uint32_t>(info.index + transient_images_offset);
      case RessourceScope::Extern:
        return utils::narrow_cast<uint32_t>(info.index + external_images_offset);
      case RessourceScope::Storage:
        return utils::narrow_cast<uint32_t>(info.index + storage_images_offset);
      default:
        break;
    }
    TR_ASSERT(false, "unreachable");
  }
  auto get_image_infos(image_ressource_handle handle) -> VkDescriptorImageInfo& {
    return descriptor_image_infos[image_index(handle)];
  }
  auto get_image_ressource(image_ressource_handle handle) -> ImageRessource& {
    return image_ressource[image_index(handle)];
  }

  [[nodiscard]] auto buffer_index(buffer_ressource_handle handle) const -> std::size_t {
    const auto info = BufferRessourceInfo::from_handle(handle);
    switch (info.scope) {
      case RessourceScope::Transient:
        return info.index + transient_buffers_offset;
      case RessourceScope::Extern:
        return info.index + external_buffers_offset;
      case RessourceScope::Storage:
        return info.index + storage_buffers_offset;
      default:
        break;
    }
    TR_ASSERT(false, "unreachable");
  }

  auto get_buffer_ressource(buffer_ressource_handle handle) -> BufferRessource& {
    return buffer_ressource[buffer_index(handle)];
  }
  template <class T, class Fn>
  void update_buffer(VmaAllocator allocator, buffer_ressource_handle id, Fn&& f) {
    T* map = nullptr;
    const auto& buf = get_buffer_ressource(id);
    vmaMapMemory(allocator, buf.alloc, reinterpret_cast<void**>(&map));

    std::forward<Fn>(f)(map);
    vmaUnmapMemory(allocator, buf.alloc);
  }
};

struct ImagePool {
  ImageDefinition infos;
  std::vector<ImageRessource> image_storage;

  auto get(ImageBuilder& f) -> ImageRessource {
    if (image_storage.empty()) {
      return f.build_image(infos);
    }
    const auto i = image_storage.back();
    image_storage.pop_back();
    return i;
  }
};

struct BufferPool {
  BufferDefinition infos;
  std::vector<BufferRessource> data_storage;

  auto get(BufferBuilder& f) -> BufferRessource {
    if (data_storage.empty()) {
      return f.build_buffer(infos);
    }
    const auto i = data_storage.back();
    data_storage.pop_back();
    return i;
  }
};

class RessourceManager {
 public:
  [[nodiscard]] auto acquire_frame_data(ImageBuilder& ib, BufferBuilder& bb) -> FrameRessourceData;

  void release_frame_data(FrameRessourceData&& frame_data_);

  auto get_image_pools() -> std::span<ImagePool> { return image_pools; }
  auto get_buffer_pools() -> std::span<BufferPool> { return buffer_pools; }

  template <class Cond, class Dtor>
  void clear_pool_if(Cond f, Dtor dtor) {
    for (auto& pool : image_pools) {
      if (f(pool.infos)) {
        for (auto& data : pool.image_storage) {
          dtor(data);
        }
        pool.image_storage.clear();
      }
    }
  }

  // Those setup functions should all be idempotent!
  auto register_storage_image(ImageRessource res) -> image_ressource_handle;
  auto register_external_image(ImageRessourceDefinition def) -> image_ressource_handle;
  auto register_transient_image(ImageRessourceDefinition def) -> image_ressource_handle;
  auto register_image(ImageRessourceDefinition def) -> image_ressource_handle {
    switch (def.scope) {
      case RessourceScope::Transient:
        return register_transient_image(def);
      case RessourceScope::Extern:
        return register_external_image(def);
      default:
        break;
    }
    TR_ASSERT(false, "unreachable");
  }

  auto register_storage_buffer(BufferRessourceDefinition def, std::optional<BufferRessource> data = std::nullopt)
      -> buffer_ressource_handle;
  auto register_external_buffer(BufferRessourceDefinition def) -> buffer_ressource_handle;
  auto register_transient_buffer(BufferRessourceDefinition def) -> buffer_ressource_handle;
  auto register_buffer(BufferRessourceDefinition def) -> buffer_ressource_handle {
    switch (def.scope) {
      case RessourceScope::Transient:
        return register_transient_buffer(def);
      case RessourceScope::Extern:
        return register_external_buffer(def);
      case RessourceScope::Storage:
        return register_storage_buffer(def);
      default:
        break;
    }
    TR_ASSERT(false, "unreachable");
  }

 private:
  auto register_image_pool(ImageDefinition def) -> std::size_t;
  auto register_buffer_pool(BufferDefinition def) -> std::size_t;

  std::vector<ImagePool> image_pools;
  std::vector<ImageRessourceDefinition> external_images;
  std::vector<std::pair<ImageRessourceId, std::size_t>> transient_images;
  std::vector<ImageRessource> storage_images;

  std::vector<BufferPool> buffer_pools;
  std::vector<BufferRessourceDefinition> external_buffers;
  std::vector<std::pair<BufferRessourceId, std::size_t>> transient_buffers;
  std::vector<std::tuple<BufferRessourceId, BufferDefinition, BufferRessource>> storage_buffers;
};

}  // namespace tr::renderer
#endif  // RESSOURCE_MANAGER_H
