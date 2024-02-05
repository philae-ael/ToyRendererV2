#pragma once

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <span>

#include "../../camera.h"
#include "../buffer.h"
#include "../debug.h"
#include "../descriptors.h"
#include "../frame.h"
#include "../mesh.h"
#include "../ressource_definition.h"
#include "frustrum_culling.h"
#include "utils/cast.h"

namespace tr::renderer {
class RessourceManager;
enum class buffer_ressource_handle : uint32_t;
enum class image_ressource_handle : uint32_t;
struct Lifetime;
struct VulkanContext;
}  // namespace tr::renderer

namespace tr::renderer {

struct Forward {
  std::array<VkDescriptorSetLayout, 2> descriptor_set_layouts{};
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkSampler shadow_map_sampler = VK_NULL_HANDLE;

  bool pcf_enable = true;
  uint8_t pcf_iter_count = 3;
  float shadow_bias = 0.0001F;

  image_ressource_handle shadow_map_handle{};
  image_ressource_handle rendered_handle{};
  image_ressource_handle depth_handle{};
  buffer_ressource_handle camera_handle{};

  static constexpr std::array set_0 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}  // camera
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_VERTEX_BIT)
          .build(),
  });

  static constexpr std::array set_1 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}  // textures, mat
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
          .descriptor_count(3)
          .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(),
      DescriptorSetLayoutBindingBuilder{}  // shadow_map
          .binding_(1)
          .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(),

  });

  void init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime);

  void start_draw(Frame &frame, VkRect2D render_area) const;
  void end_draw(VkCommandBuffer cmd) const;

  template <utils::types::range_of<const Mesh &> Range>
  void draw(Frame &frame, VkRect2D render_area, const Camera &cam, Range meshes,
            std::span<const DirectionalLight> lights, DefaultRessources default_ressources) const {
    const DebugCmdScope scope(frame.cmd.vk_cmd, "Forward");

    start_draw(frame, render_area);

    auto fr = Frustum::from_camera(cam);
    const auto camInfo = cam.cameraInfo();

    for (const auto &light : lights) {
      PushConstant data{
          light.camera_info(),
          light.color,
      };
      vkCmdPushConstants(frame.cmd.vk_cmd, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4x4),
                         sizeof(data), &data);

      for (const auto &mesh : meshes) {
        fr.transform = camInfo.viewMatrix * mesh.transform;
        draw_mesh(frame, fr, mesh, default_ressources);
      }
    }
    end_draw(frame.cmd.vk_cmd);
  }

  void draw_mesh(Frame &frame, const Frustum &frustum, const Mesh &mesh,
                 const DefaultRessources &default_ressources) const;
  auto imgui() -> bool;

  struct PushConstant {
    tr::CameraInfo info;
    glm::vec3 color;
    float padding = 0.0;
  };
};

}  // namespace tr::renderer
