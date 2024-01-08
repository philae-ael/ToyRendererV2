#pragma once

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>

#include "utils/cast.h"

namespace tr::renderer {
const std::size_t MAX_FRAMES_IN_FLIGHT = 2;

const std::array REQUIRED_DEVICE_EXTENSIONS = std::to_array<const char*>({
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,  // NEEDED FOR IMGUI
});

const std::array OPTIONAL_DEVICE_EXTENSIONS = std::to_array<const char*>({
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
});

const std::array REQUIRED_VALIDATION_LAYERS = utils::to_array<const char*>({});
const std::array OPTIONAL_VALIDATION_LAYERS = utils::to_array<const char*>({
    "VK_LAYER_KHRONOS_validation",
});

const std::array REQUIRED_INSTANCE_EXTENSIONS = utils::to_array<const char*>({});
const std::array OPTIONAL_INSTANCE_EXTENSIONS = utils::to_array<const char*>({});
}  // namespace tr::renderer
