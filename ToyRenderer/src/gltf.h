#pragma once

#include <spdlog/spdlog.h>

#include <filesystem>
#include <vector>

#include "renderer/deletion_stack.h"
#include "renderer/mesh.h"
#include "renderer/uploader.h"

namespace tr {

struct Gltf {
  static auto load_from_file(renderer::Lifetime& lifetime, tr::renderer::ImageBuilder&, tr::renderer::BufferBuilder&,
                             tr::renderer::Transferer&, tr::renderer::RessourceManager& rm,
                             const std::filesystem::path&)
      -> std::pair<std::vector<tr::renderer::Material>, std::vector<tr::renderer::Mesh>>;
};

}  // namespace tr
