#pragma once

#include <imgui.h>
#include <vulkan/vulkan_core.h>

#include <array>

#include "../descriptors.h"
#include "../frame.h"
#include "../mesh.h"
#include "utils/cast.h"

namespace tr::renderer {
struct DefaultRessources;
struct VulkanContext;

struct ShadowMap {
  std::array<VkDescriptorSetLayout, 1> descriptor_set_layouts{};
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  image_ressource_handle rendered_handle{};
  image_ressource_handle shadow_map_handle{};
  buffer_ressource_handle shadow_camera_handle{};

  static constexpr std::array set_0 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_VERTEX_BIT)
          .build(),
  });

  void init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime);

  void start_draw(Frame &frame) const;
  void end_draw(VkCommandBuffer cmd) const;

  void draw(Frame &frame, const DirectionalLight &light, std::span<const Mesh> meshes) const;

  void draw_mesh(Frame &frame, const Mesh &mesh) const;

  void imgui(RessourceManager &rm) const;
};

}  // namespace tr::renderer
