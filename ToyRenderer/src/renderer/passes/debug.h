#pragma once

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/vec3.hpp>
#include <span>
#include <vector>

#include "../descriptors.h"
#include "../vertex.h"
#include "utils/cast.h"

namespace tr::renderer {
class RessourceManager;
enum class buffer_ressource_handle : uint32_t;
struct Frame;
struct Lifetime;
struct VulkanContext;
enum class image_ressource_handle : uint32_t;
struct AABB;
}  // namespace tr::renderer

namespace tr::renderer {

struct DebugVertex {
  glm::vec3 pos;
  glm::vec3 color;

  const static std::array<VkVertexInputAttributeDescription, 2> attributes;
  const static std::array<VkVertexInputBindingDescription, 1> bindings;
};
inline constexpr std::array<VkVertexInputAttributeDescription, 2> DebugVertex::attributes =
    AttributeBuilder<2>{}
        .binding(0)
        .attribute(0, offsetof(DebugVertex, pos), VK_FORMAT_R32G32B32_SFLOAT)
        .attribute(1, offsetof(DebugVertex, color), VK_FORMAT_R32G32B32_SFLOAT)
        .build();

inline constexpr std::array<VkVertexInputBindingDescription, 1> DebugVertex::bindings =
    std::to_array<VkVertexInputBindingDescription>({
        {
            .binding = 0,
            .stride = sizeof(tr::renderer::DebugVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
    });

struct Debug {
  std::array<VkDescriptorSetLayout, 1> descriptor_set_layouts{};
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkSampler shadow_map_sampler = VK_NULL_HANDLE;

  std::vector<DebugVertex> vertices;

  image_ressource_handle rendered_handle{};
  image_ressource_handle depth_handle{};
  buffer_ressource_handle camera_handle{};
  buffer_ressource_handle debug_vertices_handle{};

  static constexpr std::array set_0 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_VERTEX_BIT)
          .build(),
  });

  static constexpr std::array bindings = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
          .descriptor_count(4)
          .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(),
      DescriptorSetLayoutBindingBuilder{}
          .binding_(1)
          .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(),

  });

  void init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime);
  void draw(Frame &frame, VkRect2D render_area);
  auto imgui() -> bool;

  static auto global() -> Debug & {
    static Debug d;
    return d;
  }

  void push_triangle(std::span<const DebugVertex, 3> v) {
    vertices.push_back(v[0]);
    vertices.push_back(v[1]);
    vertices.push_back(v[2]);
  }
  void push_aabb(const AABB &aabb);
};

}  // namespace tr::renderer
