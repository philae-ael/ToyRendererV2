#pragma once

#include <cstdint>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <vector>

#include "deletion_queue.h"
#include "ressources.h"

namespace tr::renderer {

struct Material {
  tr::renderer::ImageRessource base_color_texture{};
  std::optional<tr::renderer::ImageRessource> metallic_roughness_texture;
  std::optional<tr::renderer::ImageRessource> normal_texture;

  void defer_deletion(VmaDeletionStack& vma_deletion_stack, DeviceDeletionStack& device_deletion_stack) const {
    base_color_texture.defer_deletion(vma_deletion_stack, device_deletion_stack);
    if (metallic_roughness_texture) {
      metallic_roughness_texture->defer_deletion(vma_deletion_stack, device_deletion_stack);
    }
    if (normal_texture) {
      normal_texture->defer_deletion(vma_deletion_stack, device_deletion_stack);
    }
  }
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
