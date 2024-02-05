#pragma once

#include <vulkan/vulkan_core.h>

#include "pass.h"

namespace tr {
namespace renderer {
class RessourceManager;
struct Frame;
struct Lifetime;
struct VulkanContext;
}  // namespace renderer
}  // namespace tr

namespace tr::renderer {
struct Present {
  PassInfo pass_info;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  void init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime);
  void draw(Frame &frame, VkRect2D render_area) const;
};

}  // namespace tr::renderer
