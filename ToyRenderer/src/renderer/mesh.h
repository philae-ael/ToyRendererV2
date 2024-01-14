#pragma once

#include <cstdint>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <vector>

#include "deletion_stack.h"
#include "ressources.h"
#include "vertex.h"

namespace tr::renderer {

struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec3 tangent;
  glm::vec3 color;
  glm::vec2 uv1;
  glm::vec2 uv2;

  const static std::array<VkVertexInputAttributeDescription, 6> attributes;
  const static std::array<VkVertexInputBindingDescription, 1> bindings;
};
constexpr std::array<VkVertexInputAttributeDescription, 6> Vertex::attributes =
    AttributeBuilder<6>{}
        .binding(0)
        .attribute(0, offsetof(Vertex, pos), VK_FORMAT_R32G32B32_SFLOAT)
        .attribute(1, offsetof(Vertex, normal), VK_FORMAT_R32G32B32_SFLOAT)
        .attribute(2, offsetof(Vertex, tangent), VK_FORMAT_R32G32B32_SFLOAT)
        .attribute(3, offsetof(Vertex, color), VK_FORMAT_R32G32B32_SFLOAT)
        .attribute(4, offsetof(Vertex, uv1), VK_FORMAT_R32G32_SFLOAT)
        .attribute(5, offsetof(Vertex, uv2), VK_FORMAT_R32G32_SFLOAT)
        .build();

constexpr std::array<VkVertexInputBindingDescription, 1> Vertex::bindings =
    std::to_array<VkVertexInputBindingDescription>({
        {
            .binding = 0,
            .stride = sizeof(tr::renderer::Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
    });

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

struct DirectionalLight {
  glm::vec3 direction;
  float padding1_ = 0.0;
  glm::vec3 color;
  float padding2_ = 0.0;
};

}  // namespace tr::renderer
