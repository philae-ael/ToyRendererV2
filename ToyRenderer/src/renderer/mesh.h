#pragma once

#include <cstdint>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <vector>

#include "ressources.h"

namespace tr::renderer {

struct Mesh {
  std::string name;

  struct GPUMeshBuffers {
    BufferRessource vertices;
    std::optional<BufferRessource> indices;
  } buffers;

  struct Surface {
    uint32_t start;
    uint32_t count;
  };
  std::vector<Surface> surfaces;
  glm::mat4x4 transform = glm::identity<glm::mat4x4>();
};

}  // namespace tr::renderer
