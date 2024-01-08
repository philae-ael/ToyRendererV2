#pragma once

#include <vulkan/vulkan_core.h>

#include <format>
#include <optional>
#include <set>
#include <span>
#include <string_view>

#define VK_UNWRAP(f, ...) VK_CHECK(f(__VA_ARGS__), f)
#define VK_CHECK(result, name)                                                                               \
  do {                                                                                                       \
    VkResult __res = (result);                                                                               \
    TR_ASSERT(__res == VK_SUCCESS, "error while calling " #name " got error code {}", (std::uint32_t)__res); \
  } while (false)

namespace tr::renderer {

auto check_extensions(const char* kind, std::span<const char* const> required, std::span<const char* const> optional,
                      std::span<VkExtensionProperties> available) -> std::optional<std::set<std::string>>;

}  // namespace tr::renderer

template <>
struct std::formatter<VkResult> : formatter<std::string_view> {
  auto format(VkResult result, format_context& ctx) const -> format_context::iterator {  // NOLINT
    string_view value = "unknown";

#define CASE(name) \
  case name:       \
    value = #name; \
    break;

    switch (result) {
      CASE(VK_SUCCESS)
      CASE(VK_NOT_READY)
      CASE(VK_TIMEOUT)
      CASE(VK_EVENT_SET)
      CASE(VK_EVENT_RESET)
      CASE(VK_INCOMPLETE)
      CASE(VK_ERROR_OUT_OF_HOST_MEMORY)
      CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
      CASE(VK_ERROR_INITIALIZATION_FAILED)
      CASE(VK_ERROR_DEVICE_LOST)
      CASE(VK_ERROR_MEMORY_MAP_FAILED)
      CASE(VK_ERROR_LAYER_NOT_PRESENT)
      CASE(VK_ERROR_EXTENSION_NOT_PRESENT)
      CASE(VK_ERROR_FEATURE_NOT_PRESENT)
      CASE(VK_ERROR_INCOMPATIBLE_DRIVER)
      CASE(VK_ERROR_TOO_MANY_OBJECTS)
      CASE(VK_ERROR_FORMAT_NOT_SUPPORTED)
      CASE(VK_ERROR_FRAGMENTED_POOL)
      CASE(VK_ERROR_UNKNOWN)
      CASE(VK_ERROR_OUT_OF_POOL_MEMORY)
      CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE)
      CASE(VK_ERROR_FRAGMENTATION)
      CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS)
      CASE(VK_PIPELINE_COMPILE_REQUIRED)
      CASE(VK_ERROR_SURFACE_LOST_KHR)
      CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
      CASE(VK_SUBOPTIMAL_KHR)
      CASE(VK_ERROR_OUT_OF_DATE_KHR)
      CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
      CASE(VK_ERROR_VALIDATION_FAILED_EXT)
      CASE(VK_ERROR_INVALID_SHADER_NV)
      CASE(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR)
      CASE(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR)
      CASE(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR)
      CASE(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR)
      CASE(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR)
      CASE(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR)
      CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)
      CASE(VK_ERROR_NOT_PERMITTED_KHR)
      CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
      CASE(VK_THREAD_IDLE_KHR)
      CASE(VK_THREAD_DONE_KHR)
      CASE(VK_OPERATION_DEFERRED_KHR)
      CASE(VK_OPERATION_NOT_DEFERRED_KHR)
      CASE(VK_ERROR_COMPRESSION_EXHAUSTED_EXT)
      CASE(VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT)
      CASE(VK_RESULT_MAX_ENUM)
    }
#undef CASE
    return std::format_to(ctx.out(), "{}", value);
  }
};

template <>
struct std::formatter<VkColorSpaceKHR> : formatter<std::string_view> {
  auto format(VkColorSpaceKHR result, format_context& ctx) const -> format_context::iterator {  // NOLINT
    string_view value = "unknown";

#define CASE(name) \
  case name:       \
    value = #name; \
    break;

    switch (result) {
      CASE(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      CASE(VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT)
      CASE(VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
      CASE(VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT)
      CASE(VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT)
      CASE(VK_COLOR_SPACE_BT709_LINEAR_EXT)
      CASE(VK_COLOR_SPACE_BT709_NONLINEAR_EXT)
      CASE(VK_COLOR_SPACE_BT2020_LINEAR_EXT)
      CASE(VK_COLOR_SPACE_HDR10_ST2084_EXT)
      CASE(VK_COLOR_SPACE_DOLBYVISION_EXT)
      CASE(VK_COLOR_SPACE_HDR10_HLG_EXT)
      CASE(VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT)
      CASE(VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT)
      CASE(VK_COLOR_SPACE_PASS_THROUGH_EXT)
      CASE(VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT)
      CASE(VK_COLOR_SPACE_DISPLAY_NATIVE_AMD)
      CASE(VK_COLOR_SPACE_MAX_ENUM_KHR)
    }
#undef CASE
    return std::format_to(ctx.out(), "{}", value);
  }
};

template <>
struct std::formatter<VkFormat> : formatter<std::string_view> {
  auto format(VkFormat result, format_context& ctx) const -> format_context::iterator {  // NOLINT
    string_view value = "unknown";

#define CASE(name) \
  case name:       \
    value = #name; \
    break;

    switch (result) {
      CASE(VK_FORMAT_UNDEFINED)
      CASE(VK_FORMAT_R4G4_UNORM_PACK8)
      CASE(VK_FORMAT_R4G4B4A4_UNORM_PACK16)
      CASE(VK_FORMAT_B4G4R4A4_UNORM_PACK16)
      CASE(VK_FORMAT_R5G6B5_UNORM_PACK16)
      CASE(VK_FORMAT_B5G6R5_UNORM_PACK16)
      CASE(VK_FORMAT_R5G5B5A1_UNORM_PACK16)
      CASE(VK_FORMAT_B5G5R5A1_UNORM_PACK16)
      CASE(VK_FORMAT_A1R5G5B5_UNORM_PACK16)
      CASE(VK_FORMAT_R8_UNORM)
      CASE(VK_FORMAT_R8_SNORM)
      CASE(VK_FORMAT_R8_USCALED)
      CASE(VK_FORMAT_R8_SSCALED)
      CASE(VK_FORMAT_R8_UINT)
      CASE(VK_FORMAT_R8_SINT)
      CASE(VK_FORMAT_R8_SRGB)
      CASE(VK_FORMAT_R8G8_UNORM)
      CASE(VK_FORMAT_R8G8_SNORM)
      CASE(VK_FORMAT_R8G8_USCALED)
      CASE(VK_FORMAT_R8G8_SSCALED)
      CASE(VK_FORMAT_R8G8_UINT)
      CASE(VK_FORMAT_R8G8_SINT)
      CASE(VK_FORMAT_R8G8_SRGB)
      CASE(VK_FORMAT_R8G8B8_UNORM)
      CASE(VK_FORMAT_R8G8B8_SNORM)
      CASE(VK_FORMAT_R8G8B8_USCALED)
      CASE(VK_FORMAT_R8G8B8_SSCALED)
      CASE(VK_FORMAT_R8G8B8_UINT)
      CASE(VK_FORMAT_R8G8B8_SINT)
      CASE(VK_FORMAT_R8G8B8_SRGB)
      CASE(VK_FORMAT_B8G8R8_UNORM)
      CASE(VK_FORMAT_B8G8R8_SNORM)
      CASE(VK_FORMAT_B8G8R8_USCALED)
      CASE(VK_FORMAT_B8G8R8_SSCALED)
      CASE(VK_FORMAT_B8G8R8_UINT)
      CASE(VK_FORMAT_B8G8R8_SINT)
      CASE(VK_FORMAT_B8G8R8_SRGB)
      CASE(VK_FORMAT_R8G8B8A8_UNORM)
      CASE(VK_FORMAT_R8G8B8A8_SNORM)
      CASE(VK_FORMAT_R8G8B8A8_USCALED)
      CASE(VK_FORMAT_R8G8B8A8_SSCALED)
      CASE(VK_FORMAT_R8G8B8A8_UINT)
      CASE(VK_FORMAT_R8G8B8A8_SINT)
      CASE(VK_FORMAT_R8G8B8A8_SRGB)
      CASE(VK_FORMAT_B8G8R8A8_UNORM)
      CASE(VK_FORMAT_B8G8R8A8_SNORM)
      CASE(VK_FORMAT_B8G8R8A8_USCALED)
      CASE(VK_FORMAT_B8G8R8A8_SSCALED)
      CASE(VK_FORMAT_B8G8R8A8_UINT)
      CASE(VK_FORMAT_B8G8R8A8_SINT)
      CASE(VK_FORMAT_B8G8R8A8_SRGB)
      CASE(VK_FORMAT_A8B8G8R8_UNORM_PACK32)
      CASE(VK_FORMAT_A8B8G8R8_SNORM_PACK32)
      CASE(VK_FORMAT_A8B8G8R8_USCALED_PACK32)
      CASE(VK_FORMAT_A8B8G8R8_SSCALED_PACK32)
      CASE(VK_FORMAT_A8B8G8R8_UINT_PACK32)
      CASE(VK_FORMAT_A8B8G8R8_SINT_PACK32)
      CASE(VK_FORMAT_A8B8G8R8_SRGB_PACK32)
      CASE(VK_FORMAT_A2R10G10B10_UNORM_PACK32)
      CASE(VK_FORMAT_A2R10G10B10_SNORM_PACK32)
      CASE(VK_FORMAT_A2R10G10B10_USCALED_PACK32)
      CASE(VK_FORMAT_A2R10G10B10_SSCALED_PACK32)
      CASE(VK_FORMAT_A2R10G10B10_UINT_PACK32)
      CASE(VK_FORMAT_A2R10G10B10_SINT_PACK32)
      CASE(VK_FORMAT_A2B10G10R10_UNORM_PACK32)
      CASE(VK_FORMAT_A2B10G10R10_SNORM_PACK32)
      CASE(VK_FORMAT_A2B10G10R10_USCALED_PACK32)
      CASE(VK_FORMAT_A2B10G10R10_SSCALED_PACK32)
      CASE(VK_FORMAT_A2B10G10R10_UINT_PACK32)
      CASE(VK_FORMAT_A2B10G10R10_SINT_PACK32)
      CASE(VK_FORMAT_R16_UNORM)
      CASE(VK_FORMAT_R16_SNORM)
      CASE(VK_FORMAT_R16_USCALED)
      CASE(VK_FORMAT_R16_SSCALED)
      CASE(VK_FORMAT_R16_UINT)
      CASE(VK_FORMAT_R16_SINT)
      CASE(VK_FORMAT_R16_SFLOAT)
      CASE(VK_FORMAT_R16G16_UNORM)
      CASE(VK_FORMAT_R16G16_SNORM)
      CASE(VK_FORMAT_R16G16_USCALED)
      CASE(VK_FORMAT_R16G16_SSCALED)
      CASE(VK_FORMAT_R16G16_UINT)
      CASE(VK_FORMAT_R16G16_SINT)
      CASE(VK_FORMAT_R16G16_SFLOAT)
      CASE(VK_FORMAT_R16G16B16_UNORM)
      CASE(VK_FORMAT_R16G16B16_SNORM)
      CASE(VK_FORMAT_R16G16B16_USCALED)
      CASE(VK_FORMAT_R16G16B16_SSCALED)
      CASE(VK_FORMAT_R16G16B16_UINT)
      CASE(VK_FORMAT_R16G16B16_SINT)
      CASE(VK_FORMAT_R16G16B16_SFLOAT)
      CASE(VK_FORMAT_R16G16B16A16_UNORM)
      CASE(VK_FORMAT_R16G16B16A16_SNORM)
      CASE(VK_FORMAT_R16G16B16A16_USCALED)
      CASE(VK_FORMAT_R16G16B16A16_SSCALED)
      CASE(VK_FORMAT_R16G16B16A16_UINT)
      CASE(VK_FORMAT_R16G16B16A16_SINT)
      CASE(VK_FORMAT_R16G16B16A16_SFLOAT)
      CASE(VK_FORMAT_R32_UINT)
      CASE(VK_FORMAT_R32_SINT)
      CASE(VK_FORMAT_R32_SFLOAT)
      CASE(VK_FORMAT_R32G32_UINT)
      CASE(VK_FORMAT_R32G32_SINT)
      CASE(VK_FORMAT_R32G32_SFLOAT)
      CASE(VK_FORMAT_R32G32B32_UINT)
      CASE(VK_FORMAT_R32G32B32_SINT)
      CASE(VK_FORMAT_R32G32B32_SFLOAT)
      CASE(VK_FORMAT_R32G32B32A32_UINT)
      CASE(VK_FORMAT_R32G32B32A32_SINT)
      CASE(VK_FORMAT_R32G32B32A32_SFLOAT)
      CASE(VK_FORMAT_R64_UINT)
      CASE(VK_FORMAT_R64_SINT)
      CASE(VK_FORMAT_R64_SFLOAT)
      CASE(VK_FORMAT_R64G64_UINT)
      CASE(VK_FORMAT_R64G64_SINT)
      CASE(VK_FORMAT_R64G64_SFLOAT)
      CASE(VK_FORMAT_R64G64B64_UINT)
      CASE(VK_FORMAT_R64G64B64_SINT)
      CASE(VK_FORMAT_R64G64B64_SFLOAT)
      CASE(VK_FORMAT_R64G64B64A64_UINT)
      CASE(VK_FORMAT_R64G64B64A64_SINT)
      CASE(VK_FORMAT_R64G64B64A64_SFLOAT)
      CASE(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
      CASE(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32)
      CASE(VK_FORMAT_D16_UNORM)
      CASE(VK_FORMAT_X8_D24_UNORM_PACK32)
      CASE(VK_FORMAT_D32_SFLOAT)
      CASE(VK_FORMAT_S8_UINT)
      CASE(VK_FORMAT_D16_UNORM_S8_UINT)
      CASE(VK_FORMAT_D24_UNORM_S8_UINT)
      CASE(VK_FORMAT_D32_SFLOAT_S8_UINT)
      CASE(VK_FORMAT_BC1_RGB_UNORM_BLOCK)
      CASE(VK_FORMAT_BC1_RGB_SRGB_BLOCK)
      CASE(VK_FORMAT_BC1_RGBA_UNORM_BLOCK)
      CASE(VK_FORMAT_BC1_RGBA_SRGB_BLOCK)
      CASE(VK_FORMAT_BC2_UNORM_BLOCK)
      CASE(VK_FORMAT_BC2_SRGB_BLOCK)
      CASE(VK_FORMAT_BC3_UNORM_BLOCK)
      CASE(VK_FORMAT_BC3_SRGB_BLOCK)
      CASE(VK_FORMAT_BC4_UNORM_BLOCK)
      CASE(VK_FORMAT_BC4_SNORM_BLOCK)
      CASE(VK_FORMAT_BC5_UNORM_BLOCK)
      CASE(VK_FORMAT_BC5_SNORM_BLOCK)
      CASE(VK_FORMAT_BC6H_UFLOAT_BLOCK)
      CASE(VK_FORMAT_BC6H_SFLOAT_BLOCK)
      CASE(VK_FORMAT_BC7_UNORM_BLOCK)
      CASE(VK_FORMAT_BC7_SRGB_BLOCK)
      CASE(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK)
      CASE(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK)
      CASE(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK)
      CASE(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK)
      CASE(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK)
      CASE(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK)
      CASE(VK_FORMAT_EAC_R11_UNORM_BLOCK)
      CASE(VK_FORMAT_EAC_R11_SNORM_BLOCK)
      CASE(VK_FORMAT_EAC_R11G11_UNORM_BLOCK)
      CASE(VK_FORMAT_EAC_R11G11_SNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_4x4_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_4x4_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_5x4_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_5x4_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_5x5_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_5x5_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_6x5_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_6x5_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_6x6_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_6x6_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_8x5_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_8x5_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_8x6_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_8x6_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_8x8_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_8x8_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_10x5_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_10x5_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_10x6_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_10x6_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_10x8_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_10x8_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_10x10_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_10x10_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_12x10_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_12x10_SRGB_BLOCK)
      CASE(VK_FORMAT_ASTC_12x12_UNORM_BLOCK)
      CASE(VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
      CASE(VK_FORMAT_G8B8G8R8_422_UNORM)
      CASE(VK_FORMAT_B8G8R8G8_422_UNORM)
      CASE(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM)
      CASE(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM)
      CASE(VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM)
      CASE(VK_FORMAT_G8_B8R8_2PLANE_422_UNORM)
      CASE(VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM)
      CASE(VK_FORMAT_R10X6_UNORM_PACK16)
      CASE(VK_FORMAT_R10X6G10X6_UNORM_2PACK16)
      CASE(VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16)
      CASE(VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16)
      CASE(VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16)
      CASE(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16)
      CASE(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16)
      CASE(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16)
      CASE(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16)
      CASE(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16)
      CASE(VK_FORMAT_R12X4_UNORM_PACK16)
      CASE(VK_FORMAT_R12X4G12X4_UNORM_2PACK16)
      CASE(VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16)
      CASE(VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16)
      CASE(VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16)
      CASE(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16)
      CASE(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16)
      CASE(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16)
      CASE(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16)
      CASE(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16)
      CASE(VK_FORMAT_G16B16G16R16_422_UNORM)
      CASE(VK_FORMAT_B16G16R16G16_422_UNORM)
      CASE(VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM)
      CASE(VK_FORMAT_G16_B16R16_2PLANE_420_UNORM)
      CASE(VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM)
      CASE(VK_FORMAT_G16_B16R16_2PLANE_422_UNORM)
      CASE(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM)
      CASE(VK_FORMAT_G8_B8R8_2PLANE_444_UNORM)
      CASE(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16)
      CASE(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16)
      CASE(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM)
      CASE(VK_FORMAT_A4R4G4B4_UNORM_PACK16)
      CASE(VK_FORMAT_A4B4G4R4_UNORM_PACK16)
      CASE(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK)
      CASE(VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK)
      CASE(VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG)
      CASE(VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG)
      CASE(VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG)
      CASE(VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG)
      CASE(VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG)
      CASE(VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG)
      CASE(VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG)
      CASE(VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG)
      CASE(VK_FORMAT_R16G16_S10_5_NV)
      CASE(VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
      CASE(VK_FORMAT_A8_UNORM_KHR)
      CASE(VK_FORMAT_MAX_ENUM)
    }
#undef CASE
    return std::format_to(ctx.out(), "{}", value);
  }
};

template <>
struct std::formatter<VkObjectType> : formatter<std::string_view> {
  auto format(VkObjectType result, format_context& ctx) const -> format_context::iterator {  // NOLINT
    string_view value = "unknown";

#define CASE(name) \
  case name:       \
    value = #name; \
    break;

    switch (result) {
      CASE(VK_OBJECT_TYPE_UNKNOWN)
      CASE(VK_OBJECT_TYPE_INSTANCE)
      CASE(VK_OBJECT_TYPE_PHYSICAL_DEVICE)
      CASE(VK_OBJECT_TYPE_DEVICE)
      CASE(VK_OBJECT_TYPE_QUEUE)
      CASE(VK_OBJECT_TYPE_SEMAPHORE)
      CASE(VK_OBJECT_TYPE_COMMAND_BUFFER)
      CASE(VK_OBJECT_TYPE_FENCE)
      CASE(VK_OBJECT_TYPE_DEVICE_MEMORY)
      CASE(VK_OBJECT_TYPE_BUFFER)
      CASE(VK_OBJECT_TYPE_IMAGE)
      CASE(VK_OBJECT_TYPE_EVENT)
      CASE(VK_OBJECT_TYPE_QUERY_POOL)
      CASE(VK_OBJECT_TYPE_BUFFER_VIEW)
      CASE(VK_OBJECT_TYPE_IMAGE_VIEW)
      CASE(VK_OBJECT_TYPE_SHADER_MODULE)
      CASE(VK_OBJECT_TYPE_PIPELINE_CACHE)
      CASE(VK_OBJECT_TYPE_PIPELINE_LAYOUT)
      CASE(VK_OBJECT_TYPE_RENDER_PASS)
      CASE(VK_OBJECT_TYPE_PIPELINE)
      CASE(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
      CASE(VK_OBJECT_TYPE_SAMPLER)
      CASE(VK_OBJECT_TYPE_DESCRIPTOR_POOL)
      CASE(VK_OBJECT_TYPE_DESCRIPTOR_SET)
      CASE(VK_OBJECT_TYPE_FRAMEBUFFER)
      CASE(VK_OBJECT_TYPE_COMMAND_POOL)
      CASE(VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION)
      CASE(VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE)
      CASE(VK_OBJECT_TYPE_PRIVATE_DATA_SLOT)
      CASE(VK_OBJECT_TYPE_SURFACE_KHR)
      CASE(VK_OBJECT_TYPE_SWAPCHAIN_KHR)
      CASE(VK_OBJECT_TYPE_DISPLAY_KHR)
      CASE(VK_OBJECT_TYPE_DISPLAY_MODE_KHR)
      CASE(VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT)
      CASE(VK_OBJECT_TYPE_VIDEO_SESSION_KHR)
      CASE(VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR)
      CASE(VK_OBJECT_TYPE_CU_MODULE_NVX)
      CASE(VK_OBJECT_TYPE_CU_FUNCTION_NVX)
      CASE(VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT)
      CASE(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR)
      CASE(VK_OBJECT_TYPE_VALIDATION_CACHE_EXT)
      CASE(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV)
      CASE(VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL)
      CASE(VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR)
      CASE(VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV)
      CASE(VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA)
      CASE(VK_OBJECT_TYPE_MICROMAP_EXT)
      CASE(VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV)
      CASE(VK_OBJECT_TYPE_SHADER_EXT)
      CASE(VK_OBJECT_TYPE_MAX_ENUM)
    }
#undef CASE
    return std::format_to(ctx.out(), "{}", value);
  }
};

template <>
struct std::formatter<VkPresentModeKHR> : formatter<std::string_view> {
  auto format(VkPresentModeKHR result, format_context& ctx) const -> format_context::iterator {  // NOLINT
    string_view value = "unknown";

#define CASE(name) \
  case name:       \
    value = #name; \
    break;

    switch (result) {
      CASE(VK_PRESENT_MODE_IMMEDIATE_KHR)
      CASE(VK_PRESENT_MODE_MAILBOX_KHR)
      CASE(VK_PRESENT_MODE_FIFO_KHR)
      CASE(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
      CASE(VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR)
      CASE(VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR)
      CASE(VK_PRESENT_MODE_MAX_ENUM_KHR)
      break;
    }
#undef CASE
    return std::format_to(ctx.out(), "{}", value);
  }
};
