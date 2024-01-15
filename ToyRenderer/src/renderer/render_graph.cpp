#include "render_graph.h"

#include <array>

#include "../camera.h"
#include "mesh.h"
#include "passes/shadow_map.h"
#include "ressources.h"
#include "timeline_info.h"
#include "vulkan_engine.h"

void tr::renderer::RenderGraph::draw(Frame& frame, std::span<const Mesh> meshes, const Camera& camera) {
  frame.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_DRAW_TOP);
  auto swapchain_extent = frame.frm.get_image(ImageRessourceId::Swapchain).extent;

  frame.write_gpu_timestamp(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_TOP);

  frame.frm.update_buffer<CameraInfo>(frame.ctx->allocator, BufferRessourceId::Camera,
                                      [&](CameraInfo* info) { *info = camera.cameraInfo(); });

  passes.gbuffer.draw(frame, {{0, 0}, swapchain_extent}, meshes, default_ressources);

  frame.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_GBUFFER_BOTTOM);
  frame.write_gpu_timestamp(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, GPU_TIMESTAMP_INDEX_GBUFFER_BOTTOM);

  const std::array lights = std::to_array<DirectionalLight>({
      {
          .direction = glm::normalize(glm::vec3{1, 5, -3}),
          .color = {2, 2, 2},
      },
  });
  passes.shadow_map.draw(frame, lights[0], meshes);

  frame.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_SHADOW_BOTTOM);
  frame.write_gpu_timestamp(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, GPU_TIMESTAMP_INDEX_SHADOW_BOTTOM);

  passes.deferred.draw(frame, {{0, 0}, swapchain_extent}, lights);

  frame.write_gpu_timestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, GPU_TIMESTAMP_INDEX_BOTTOM);
  frame.write_cpu_timestamp(CPU_TIMESTAMP_INDEX_DRAW_BOTTOM);
}

void tr::renderer::RenderGraph::init(tr::renderer::VulkanEngine& engine, Transferer& t, ImageBuilder& ib,
                                     BufferBuilder& /*bb*/) {
  passes.gbuffer = GBuffer::init(engine.device.vk_device, engine.swapchain, engine.frame_deletion_stacks.device);
  passes.gbuffer.defer_deletion(engine.global_deletion_stacks.device);

  passes.shadow_map = ShadowMap::init(engine.device.vk_device, engine.swapchain, engine.frame_deletion_stacks.device);
  passes.shadow_map.defer_deletion(engine.global_deletion_stacks.device);

  passes.deferred = Deferred::init(engine.device.vk_device, engine.swapchain, engine.frame_deletion_stacks.device);
  passes.deferred.defer_deletion(engine.global_deletion_stacks.device);

  {
    const VkSamplerCreateInfo sampler_create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 0,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0,
        .maxLod = 0,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    VK_UNWRAP(vkCreateSampler, engine.device.vk_device, &sampler_create_info, nullptr, &default_ressources.sampler);
    engine.global_deletion_stacks.device.defer_deletion(DeviceHandle::Sampler, default_ressources.sampler);
  }

  {
    default_ressources.metallic_roughness = ib.build_image({
        .flags = 0,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .size = VkExtent2D{1, 1},
        .format = VK_FORMAT_R8G8_UNORM,
        .debug_name = "default metallic_roughness_texture",
    });
    default_ressources.metallic_roughness.defer_deletion(engine.global_deletion_stacks.allocator,
                                                         engine.global_deletion_stacks.device);

    default_ressources.normal_map = ib.build_image({
        .flags = 0,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .size = VkExtent2D{1, 1},
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .debug_name = "default normal_texture",
    });
    default_ressources.normal_map.defer_deletion(engine.global_deletion_stacks.allocator,
                                                 engine.global_deletion_stacks.device);

    ImageMemoryBarrier::submit<2>(
        t.cmd.vk_cmd, {{
                          default_ressources.metallic_roughness.prepare_barrier(tr::renderer::SyncImageTransfer),
                          default_ressources.normal_map.prepare_barrier(tr::renderer::SyncImageTransfer),
                      }});

    {
      std::array<uint8_t, 4> data{0xFF, 0xFF, 0xFF, 0xFF};
      t.upload_image(default_ressources.metallic_roughness, {{0, 0}, {1, 1}}, std::as_bytes(std::span(data)), 2);
    }

    {
      std::array<float, 4> data{0.0, 0.0, 1.0, 0.0};
      t.upload_image(default_ressources.normal_map, {{0, 0}, {1, 1}}, std::as_bytes(std::span(data)), 16);
    }

    tr::renderer::ImageMemoryBarrier::submit<2>(
        t.graphics_cmd.vk_cmd,
        {{
            default_ressources.metallic_roughness.prepare_barrier(tr::renderer::SyncFragmentShaderReadOnly),
            default_ressources.normal_map.prepare_barrier(tr::renderer::SyncFragmentShaderReadOnly),
        }});
  }
}
