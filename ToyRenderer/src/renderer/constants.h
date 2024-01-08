#pragma once

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>

namespace tr::renderer {
const std::size_t MAX_FRAMES_IN_FLIGHT = 2;

const std::array<const char*, 2> REQUIRED_DEVICE_EXTENSIONS{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
};

const std::array<const char*, 1> OPTIONAL_DEVICE_EXTENSIONS{
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
};

const std::array<const char*, 0> REQUIRED_VALIDATION_LAYERS{};
const std::array<const char*, 1> OPTIONAL_VALIDATION_LAYERS{
    "VK_LAYER_KHRONOS_validation",
};

const std::array<const char*, 0> REQUIRED_INSTANCE_EXTENSIONS{};
const std::array<const char*, 1> OPTIONAL_INSTANCE_EXTENSIONS{
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
};
}  // namespace tr::renderer
