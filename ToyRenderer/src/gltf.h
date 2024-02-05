#pragma once

#include <string_view>
#include <utility>
#include <vector>

namespace tr {
namespace renderer {
class BufferBuilder;
class ImageBuilder;
class RessourceManager;
struct Lifetime;
struct Material;
struct Mesh;
struct Transferer;
}  // namespace renderer

struct Gltf {
  static auto load_from_file(renderer::Lifetime& lifetime, tr::renderer::ImageBuilder&, tr::renderer::BufferBuilder&,
                             tr::renderer::Transferer&, tr::renderer::RessourceManager& rm, std::string_view path)
      -> std::pair<std::vector<tr::renderer::Material>, std::vector<tr::renderer::Mesh>>;
};

}  // namespace tr
