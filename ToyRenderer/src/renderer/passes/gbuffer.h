#pragma once

#include <vulkan/vulkan_core.h>

#include "../../camera.h"
#include "../buffer.h"
#include "../debug.h"
#include "../frame.h"
#include "../ressource_definition.h"
#include "frustrum_culling.h"
#include "pass.h"

namespace tr::renderer {
class RessourceManager;
struct Lifetime;
struct Mesh;
struct VulkanContext;
}  // namespace tr::renderer

namespace tr::renderer {

struct GBuffer {
  PassInfo pass_info;
  VkPipeline pipeline = VK_NULL_HANDLE;

  void init(Lifetime &lifetime, VulkanContext &ctx, RessourceManager &rm, Lifetime &setup_lifetime);

  void start_draw(Frame &frame, VkRect2D render_area, const DefaultRessources &default_ressources) const;
  void end_draw(VkCommandBuffer cmd) const;

  template <utils::types::range_of<const Mesh &> Range>
  void draw(Frame &frame, VkRect2D render_area, const Camera &cam, Range meshes,
            DefaultRessources default_ressources) const {
    const DebugCmdScope scope(frame.cmd.vk_cmd, "GBuffer");

    start_draw(frame, render_area, default_ressources);

    // TODO: not needed every frame ! only when camera changes
    auto fr = Frustum::from_camera(cam);
    const auto camInfo = cam.cameraInfo();

    for (const auto &mesh : meshes) {
      draw_mesh(frame, fr.transform(camInfo.viewMatrix * mesh.transform), mesh, default_ressources);
    }
    end_draw(frame.cmd.vk_cmd);
  }

  void draw_mesh(Frame &frame, const Frustum &frustum, const Mesh &mesh,
                 const DefaultRessources &default_ressources) const;
};

}  // namespace tr::renderer
