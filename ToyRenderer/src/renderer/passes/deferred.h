#pragma once

#include <stdint.h>
#include <vulkan/vulkan_core.h>

#include <span>

#include "pass.h"

namespace tr {
namespace renderer {
class RessourceManager;
struct DirectionalLight;
struct Frame;
struct Lifetime;
struct VulkanContext;
}  // namespace renderer
}  // namespace tr

namespace tr::renderer {

struct Deferred {
  PassInfo pass_info;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  bool pcf_enable = true;
  uint8_t pcf_iter_count = 3;
  float shadow_bias = 0.0001F;

  void init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime);
  void draw(Frame &frame, VkRect2D render_area, std::span<const DirectionalLight> lights) const;
  auto imgui() -> bool;
};

}  // namespace tr::renderer
