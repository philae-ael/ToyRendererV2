#pragma once

#include <vulkan/vulkan_core.h>

#include <array>

#include "../camera.h"
#include "ressources.h"
#include "utils/cast.h"

namespace tr::renderer {

CVAR_FLOAT(internal_resolution_scale, 1.0)
CVAR_EXTENT2D(shadow_map_extent, 1024, 1024)

static constexpr std::array image_definition = utils::to_array<tr::renderer::ImageRessourceDefinition>({
    {
        {
            .flags = IMAGE_OPTION_FLAG_EXTERNAL_BIT,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .size = FramebufferExtent{},
            .format = FramebufferFormat{},
            .debug_name = "swapchain",
        },
        ImageRessourceId::Swapchain,
    },
    {
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .size = InternalResolution{},
            .format = FramebufferFormat{},
            .debug_name = "rendered",
        },
        ImageRessourceId::Rendered,
    },
    {
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .size = InternalResolution{},
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .debug_name = "GBuffer0 (RGB: color, A: roughness)",
        },
        ImageRessourceId::GBuffer0,
    },
    {
        // RGB: normal, A: metallic
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .size = InternalResolution{},
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .debug_name = "GBuffer1 (RGB: normal, A: metallic)",
        },
        ImageRessourceId::GBuffer1,
    },
    {
        // RGB: ViewDir
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .size = InternalResolution{},
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .debug_name = "GBuffer2 (RGB: viewDir)",
        },
        ImageRessourceId::GBuffer2,
    },
    {
        // RGB: ViewDir
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .size = InternalResolution{},
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .debug_name = "GBuffer3 (RGB: Position)",
        },
        ImageRessourceId::GBuffer3,
    },
    {
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .size = InternalResolution{},
            .format = VK_FORMAT_D16_UNORM,
            .debug_name = "Depth",
        },
        ImageRessourceId::Depth,
    },
    {
        {
            .flags = 0,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .size = shadow_map_extent,
            .format = VK_FORMAT_D16_UNORM,
            .debug_name = "Shadow Map",
        },
        ImageRessourceId::ShadowMap,
    },
});

static constexpr std::array
    buffer_definition =
        utils::to_array<tr::renderer::BufferRessourceDefinition>(
            {
                {
                    .definition =
                        {
                            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            .size = utils::align<uint32_t>(utils::narrow_cast<uint32_t>(sizeof(CameraInfo)), 256),
                            .flags = BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT,
                            .debug_name = "camera uniform",
                        },
                    .id = BufferRessourceId::Camera,
                },
                {
                    .definition =
                        {
                            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            .size = utils::align<uint32_t>(utils::narrow_cast<uint32_t>(sizeof(CameraInfo)), 256),
                            .flags = BUFFER_OPTION_FLAG_CPU_TO_GPU_BIT,
                            .debug_name = "shadow camera uniforms",
                        },
                    .id = BufferRessourceId::ShadowCamera,
                },
            });
}  // namespace tr::renderer
