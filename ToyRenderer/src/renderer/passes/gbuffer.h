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

struct GBuffer {
  std::array<VkDescriptorSetLayout, 2> descriptor_set_layouts{};
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

  static constexpr std::array set_1 = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
          .descriptor_count(3)
          .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(),
  });

  static auto init(Lifetime &setup_lifetime, Lifetime &lifetime, VkDevice &device, const RessourceManager &rm,
                   const Swapchain &Swapchain) -> GBuffer;

  void start_draw(Frame &frame, VkRect2D render_area) const;
  void end_draw(VkCommandBuffer cmd) const;

  void draw(Frame &frame, VkRect2D render_area, std::span<const Mesh> meshes, DefaultRessources default_ressources);

  void draw_mesh(Frame &frame, const Mesh &mesh, const DefaultRessources &default_ressources);
};

}  // namespace tr::renderer
