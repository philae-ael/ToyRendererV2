#pragma once

#include <spdlog/spdlog.h>

#include <filesystem>
#include <vector>

#include "renderer/mesh.h"
#include "renderer/uploader.h"

namespace tr {

struct Gltf {
  static auto load_from_file(tr::renderer::ImageBuilder&, tr::renderer::BufferBuilder&, tr::renderer::Transferer&,
                             const std::filesystem::path&) -> std::vector<renderer::Mesh>;
};

}  // namespace tr