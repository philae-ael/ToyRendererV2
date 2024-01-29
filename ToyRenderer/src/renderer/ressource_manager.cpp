#include "ressource_manager.h"

#include <cstddef>
#include <optional>

#include "ressources.h"

template <class T, class D, class Proj, class Ctor>
auto find_or_push_back(std::vector<T>& v, D d, Proj proj, Ctor ctor) -> std::size_t {
  const auto it = std::ranges::find(v, d, proj);
  if (it != v.end()) {
    return static_cast<std::size_t>(std::distance(v.begin(), it));
  }

  v.push_back(ctor(d));
  return v.size() - 1;
}

auto tr::renderer::RessourceManager::acquire_frame_data(ImageBuilder& ib, BufferBuilder& bb) -> FrameRessourceData {
  FrameRessourceData frame_data;

  frame_data.images.reserve(transient_images.size() + storage_images.size() + external_images.size());
  frame_data.image_ressource.reserve(transient_images.size() + storage_images.size() + external_images.size());
  frame_data.transient_images_offset = frame_data.images.size();
  for (const auto& [id, pool_id] : transient_images) {
    const auto data = image_pools[pool_id].get(ib);
    frame_data.images.emplace_back(data.image);
    frame_data.image_ressource.emplace_back(data);
  }
  frame_data.storage_images_offset = frame_data.images.size();
  for (const auto& [id, _, data] : storage_images) {
    frame_data.images.emplace_back(data.image);
    frame_data.image_ressource.emplace_back(data);
  }
  frame_data.external_images_offset = frame_data.images.size();
  frame_data.images.resize(frame_data.images.size() + external_images.size());
  frame_data.image_ressource.resize(frame_data.image_ressource.size() + external_images.size());

  frame_data.buffer_ressource.reserve(transient_buffers.size() + storage_buffers.size() + external_buffers.size());
  frame_data.transient_buffers_offset = frame_data.buffer_ressource.size();
  for (const auto& [id, pool_id] : transient_buffers) {
    const auto data = buffer_pools[pool_id].get(bb);
    frame_data.buffer_ressource.emplace_back(data);
  }
  frame_data.storage_buffers_offset = frame_data.buffer_ressource.size();
  for (const auto& [id, _, data] : storage_buffers) {
    frame_data.buffer_ressource.emplace_back(data);
  }
  frame_data.external_buffers_offset = frame_data.buffer_ressource.size();
  frame_data.buffer_ressource.resize(frame_data.buffer_ressource.size() + external_buffers.size());

  return frame_data;
}

void tr::renderer::RessourceManager::release_frame_data(FrameRessourceData&& frame_data_) {
  FrameRessourceData frame_data = std::move(frame_data_);

  for (std::size_t i = 0; i < transient_images.size(); i++) {
    auto& pool = image_pools[transient_images[i].second];
    pool.image_storage.push_back(frame_data.image_ressource[frame_data.transient_images_offset + i]);
  }

  for (std::size_t i = 0; i < transient_buffers.size(); i++) {
    auto& pool = buffer_pools[transient_buffers[i].second];
    pool.data_storage.push_back(frame_data.buffer_ressource[frame_data.transient_buffers_offset + i]);
  }
}

auto tr::renderer::RessourceManager::register_storage_image(ImageRessourceDefinition def,
                                                            std::optional<ImageRessource> data)
    -> image_ressource_handle {
  const auto i = find_or_push_back(
      storage_images, def.id, [](const auto& s) { return std::get<ImageRessourceId>(s); },
      [def](const auto id) {
        return std::tuple{id, def.definition, ImageRessource{}};
      });

  if (data) {
    std::get<ImageRessource>(storage_images[i]) = *data;
  }

  return ImageRessourceInfo{
      static_cast<uint16_t>(i),
      RessourceScope::Storage,
  }
      .into_handle();
}

auto tr::renderer::RessourceManager::register_external_image(ImageRessourceDefinition def) -> image_ressource_handle {
  const auto i =
      find_or_push_back(external_images, def.id, &ImageRessourceDefinition::id, [def](auto& /*id*/) { return def; });

  return ImageRessourceInfo{
      static_cast<uint16_t>(i),
      RessourceScope::Extern,
  }
      .into_handle();
}

auto tr::renderer::RessourceManager::register_transient_image(ImageRessourceDefinition def) -> image_ressource_handle {
  const auto i = find_or_push_back(
      transient_images, def.id, [](auto& s) { return s.first; },
      [this, def](const auto id) {
        return std::pair{id, register_image_pool(def.definition)};
      });

  return ImageRessourceInfo{
      static_cast<uint16_t>(i),
      RessourceScope::Transient,
  }
      .into_handle();
}

auto tr::renderer::RessourceManager::register_image_pool(ImageDefinition def) -> std::size_t {
  return find_or_push_back(image_pools, def, &ImagePool::infos, [](const auto d) { return ImagePool{d, {}}; });
}

auto tr::renderer::RessourceManager::register_storage_buffer(BufferRessourceDefinition def,
                                                             std::optional<BufferRessource> data)
    -> buffer_ressource_handle {
  const auto i = find_or_push_back(
      storage_buffers, def.id, [](const auto& s) { return std::get<BufferRessourceId>(s); },
      [def](const auto id) {
        return std::tuple{id, def.definition, BufferRessource{}};
      });

  if (data) {
    std::get<BufferRessource>(storage_buffers[i]) = *data;
  }

  return BufferRessourceInfo{
      static_cast<uint16_t>(i),
      RessourceScope::Storage,
  }
      .into_handle();
}

auto tr::renderer::RessourceManager::register_external_buffer(BufferRessourceDefinition def)
    -> buffer_ressource_handle {
  const auto i =
      find_or_push_back(external_buffers, def.id, &BufferRessourceDefinition::id, [def](auto& /*id*/) { return def; });

  return BufferRessourceInfo{
      static_cast<uint16_t>(i),
      RessourceScope::Extern,
  }
      .into_handle();
}

auto tr::renderer::RessourceManager::register_transient_buffer(BufferRessourceDefinition def)
    -> buffer_ressource_handle {
  const auto i = find_or_push_back(
      transient_buffers, def.id, [](auto& s) { return s.first; },
      [this, def](const auto id) {
        return std::pair{id, register_buffer_pool(def.definition)};
      });

  return BufferRessourceInfo{
      static_cast<uint16_t>(i),
      RessourceScope::Transient,
  }
      .into_handle();
}

auto tr::renderer::RessourceManager::register_buffer_pool(BufferDefinition def) -> std::size_t {
  return find_or_push_back(buffer_pools, def, &BufferPool::infos, [](const auto d) { return BufferPool{d, {}}; });
}
