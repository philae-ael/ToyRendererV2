#pragma once

#include <vulkan/vulkan_core.h>

#include "../camera.h"
#include "mesh.h"
#include "passes/deferred.h"
#include "passes/gbuffer.h"
#include "ressources.h"
#include "uploader.h"

namespace tr::renderer {
struct DefaultRessources {
  VkSampler sampler = VK_NULL_HANDLE;
  ImageRessource metallic_roughness{};
  ImageRessource normal_map{};
};
struct RenderGraph {
  void init(VulkanEngine& engine, Transferer& t, ImageBuilder& ib, BufferBuilder& bb);
  void draw(Frame& frame, std::span<const Mesh> meshes, const Camera& camera);

  struct {
    GBuffer gbuffer;
    Deferred deferred;
  } passes;

  DefaultRessources default_ressources;
};

}  // namespace tr::renderer
