#pragma once

#include <vulkan/vulkan_core.h>

#include <array>

#include "../descriptors.h"
#include "../frame.h"
#include "../mesh.h"
#include "../ressources.h"
#include "utils/cast.h"

namespace tr::renderer {
struct DefaultRessources;

struct ShadowMap {
  std::array<VkDescriptorSetLayout, 1> descriptor_set_layouts{};
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  static constexpr std::array set_0 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_VERTEX_BIT)
          .build(),
  });

  static auto init(Lifetime &setup_lifetime, Lifetime &lifetime, VkDevice &device, const RessourceManager &rm,
                   const Swapchain &Swapchain) -> ShadowMap;

  void start_draw(Frame &frame) const;
  void end_draw(VkCommandBuffer cmd) const;

  void draw(Frame &frame, const DirectionalLight &light, std::span<const Mesh> meshes);

  void draw_mesh(Frame &frame, const Mesh &mesh) const;
};

}  // namespace tr::renderer
