#pragma once

#include <vulkan/vulkan_core.h>

#include "../camera.h"
#include "mesh.h"
#include "passes/deferred.h"
#include "passes/forward.h"
#include "passes/gbuffer.h"
#include "passes/present.h"
#include "passes/shadow_map.h"
#include "ressource_manager.h"
#include "uploader.h"
#include "vulkan_engine.h"

namespace tr::renderer {

struct RenderGraph {
  void reinit_passes(tr::renderer::VulkanEngine& engine);
  void init(VulkanEngine& engine, Transferer& t);
  void draw(Frame& frame, std::span<const Mesh> meshes, const Camera& camera) const;

  void imgui(VulkanEngine&);

  struct {
    GBuffer gbuffer;
    ShadowMap shadow_map;
    Deferred deferred;
    Forward forward;
    Present present;
  } passes;

  DefaultRessources default_ressources{};

  image_ressource_handle swapchain_handle{};
  image_ressource_handle rendered_handle{};
  buffer_ressource_handle camera_handle{};
  buffer_ressource_handle shadow_camera_handle{};
};

}  // namespace tr::renderer
