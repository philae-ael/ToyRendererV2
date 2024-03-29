#pragma once

#include <cstdint>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <optional>
#include <vector>

#include "../camera.h"
#include "ressources.h"
#include "vertex.h"

namespace tr::renderer {
enum class image_ressource_handle : uint32_t;
enum class buffer_ressource_handle : uint32_t;

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
inline constexpr std::array<VkVertexInputAttributeDescription, 6> Vertex::attributes =
    AttributeBuilder<6>{}
        .binding(0)
        .attribute(0, offsetof(Vertex, pos), VK_FORMAT_R32G32B32_SFLOAT)
        .attribute(1, offsetof(Vertex, normal), VK_FORMAT_R32G32B32_SFLOAT)
        .attribute(2, offsetof(Vertex, tangent), VK_FORMAT_R32G32B32_SFLOAT)
        .attribute(3, offsetof(Vertex, color), VK_FORMAT_R32G32B32_SFLOAT)
        .attribute(4, offsetof(Vertex, uv1), VK_FORMAT_R32G32_SFLOAT)
        .attribute(5, offsetof(Vertex, uv2), VK_FORMAT_R32G32_SFLOAT)
        .build();

inline constexpr std::array<VkVertexInputBindingDescription, 1> Vertex::bindings =
    std::to_array<VkVertexInputBindingDescription>({
        {
            .binding = 0,
            .stride = sizeof(tr::renderer::Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
    });

struct MaterialHandles {
  image_ressource_handle albedo_handle;
  std::optional<image_ressource_handle> normal_handle;
  std::optional<image_ressource_handle> metallic_roughness_handle;
};
struct Material {
  tr::renderer::ImageRessource albedo_texture{};
  std::optional<tr::renderer::ImageRessource> metallic_roughness_texture;
  std::optional<tr::renderer::ImageRessource> normal_texture;
  MaterialHandles handles{};
};

struct AABB {
  glm::vec3 min;
  glm::vec3 max;
};

struct GeoSurface {
  uint32_t start;
  uint32_t count;
  MaterialHandles material;
  AABB bounding_box;
};

struct Mesh {
  std::string name;

  struct GPUMeshBuffers {
    BufferRessource vertices;
    std::optional<BufferRessource> indices;
  } buffers;

  std::vector<GeoSurface> surfaces;
  glm::mat4x4 transform = glm::identity<glm::mat4x4>();
};

struct DirectionalLight {
  glm::vec3 direction;
  float padding1_ = 0.0;
  glm::vec3 color;
  float padding2_ = 0.0;

  [[nodiscard]] auto camera_info() const -> CameraInfo {
    const glm::vec3 pos = 40.F * direction;
    auto projMat = glm::ortho(-10., 10., -10., 10., 0.1, 50.);
    projMat[1][1] *= -1;
    return {
        .projMatrix = projMat,
        .viewMatrix = glm::lookAt(pos, -direction, glm::vec3(0, 1, 0)),
        .cameraPosition = pos,
    };
  }
};

}  // namespace tr::renderer
