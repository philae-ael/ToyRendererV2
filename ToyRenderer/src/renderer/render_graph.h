#pragma once

#include <cstdint>
#include <span>

#include "passes/deferred.h"
#include "passes/forward.h"
#include "passes/gbuffer.h"
#include "passes/present.h"
#include "passes/shadow_map.h"
#include "passes/ssao.h"
#include "ressource_definition.h"

namespace tr {
namespace renderer {
class VulkanEngine;
enum class buffer_ressource_handle : uint32_t;
struct Frame;
struct Mesh;
struct Transferer;
enum class image_ressource_handle : uint32_t;
}  // namespace renderer
struct Camera;
}  // namespace tr

namespace tr::renderer {

class RenderGraph {
 public:
  void init(VulkanEngine& engine, Transferer& t);
  void draw(Frame& frame, std::span<const Mesh> meshes, const Camera& camera) const;

  void imgui(VulkanEngine&);

 private:
  void reinit_passes(tr::renderer::VulkanEngine& engine);
  struct {
    GBuffer gbuffer;
    SSAO ssao;
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
