#pragma once

#include <vulkan/vulkan_core.h>

#include <array>

#include "../context.h"
#include "../descriptors.h"
#include "../frame.h"
#include "../ressources.h"
#include "utils/cast.h"

namespace tr::renderer {
struct Present {
  std::array<VkDescriptorSetLayout, 1> descriptor_set_layouts{};
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  VkSampler scalling_sampler = VK_NULL_HANDLE;

  static constexpr std::array bindings = utils::to_array({
      DescriptorSetLayoutBindingBuilder{}
          .binding_(0)
          .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
          .descriptor_count(1)
          .stages(VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(),

  });

  static auto init(Lifetime &lifetime, VulkanContext &ctx, const RessourceManager &rm, Lifetime &setup_lifetime)
      -> Present;

  void draw(Frame &frame, VkRect2D render_area) const;
};

}  // namespace tr::renderer
