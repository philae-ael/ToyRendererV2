#pragma once

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <array>

#include "../context.h"
#include "../descriptors.h"
#include "../frame.h"
#include "../mesh.h"
#include "../ressource_definition.h"
#include "../ressources.h"
#include "../vulkan_engine.h"
#include "frustrum_culling.h"
#include "utils/cast.h"

namespace tr::renderer {

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

  static auto init(Lifetime &lifetime, VulkanContext &ctx, const RessourceManager &rm, Lifetime &setup_lifetime)
      -> GBuffer;

  void start_draw(Frame &frame, VkRect2D render_area) const;
  void end_draw(VkCommandBuffer cmd) const;

  template <utils::types::range_of<const Mesh &> Range>
  void draw(Frame &frame, VkRect2D render_area, const Camera &cam, Range meshes, DefaultRessources default_ressources) {
    const DebugCmdScope scope(frame.cmd.vk_cmd, "GBuffer");

    start_draw(frame, render_area);

    const auto camInfo = cam.cameraInfo();
    frame.frm.update_buffer<CameraInfo>(frame.ctx->allocator, BufferRessourceId::Camera,
                                        [&](CameraInfo *info) { *info = camInfo; });

    // TODO: not needed every frame ! only when camera changes
    auto fr = Frustum::from_camera(cam);

    for (const auto &mesh : meshes) {
      // TODO: find something smarter to not have to do so many mat mul
      fr.transform = camInfo.viewMatrix * mesh.transform;
      draw_mesh(frame, fr, mesh, default_ressources);
    }
    end_draw(frame.cmd.vk_cmd);
  }

  void draw_mesh(Frame &frame, const Frustum &frustum, const Mesh &mesh,
                 const DefaultRessources &default_ressources) const;
};

}  // namespace tr::renderer
