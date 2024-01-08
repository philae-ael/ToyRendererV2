#pragma once

#include <cstdint>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <memory>
#include <vector>

#include "ressources.h"

namespace tr::renderer {

struct Material {
  tr::renderer::ImageRessource base_color_texture;
};

struct Mesh {
  std::string name;

  struct GPUMeshBuffers {
    BufferRessource vertices;
    std::optional<BufferRessource> indices;
  } buffers;

  struct Surface {
    uint32_t start;
    uint32_t count;
    std::shared_ptr<Material> material;
  };
  std::vector<Surface> surfaces;
  glm::mat4x4 transform = glm::identity<glm::mat4x4>();
};

}  // namespace tr::renderer
