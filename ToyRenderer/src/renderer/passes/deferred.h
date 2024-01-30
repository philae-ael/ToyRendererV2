#pragma once

#include <shaderc/shaderc.h>
#include <vulkan/vulkan_core.h>

#include <array>

#include "../context.h"
#include "../descriptors.h"
#include "../frame.h"
#include "../mesh.h"
#include "pass.h"
#include "utils/cast.h"

namespace tr::renderer {

struct Deferred {
  PassInfo pass_info;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkSampler shadow_map_sampler = VK_NULL_HANDLE;

  bool pcf_enable = true;
  uint8_t pcf_iter_count = 3;
  float shadow_bias = 0.0001F;

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
  void draw(Frame &frame, VkRect2D render_area, std::span<const DirectionalLight> lights) const;
  auto imgui() -> bool;
};

}  // namespace tr::renderer
