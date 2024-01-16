#include "pipeline.h"

#include <utils/asset.h>
#include <utils/cast.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <filesystem>

#include "utils.h"

auto tr::renderer::Shader::init_from_filename(Lifetime& lifetime, VkDevice device, const std::filesystem::path& path)
    -> Shader {
  const auto data = read_file<uint32_t>(path.string());
  return init_from_src(lifetime, device, data);
}
auto tr::renderer::Shader::init_from_src(Lifetime& lifetime, VkDevice device, std::span<const uint32_t> module)
    -> Shader {
  Shader shader{};

  const VkShaderModuleCreateInfo shader_module_create_info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .codeSize = utils::narrow_cast<uint32_t>(module.size_bytes()),
      .pCode = module.data(),
  };

  VK_UNWRAP(vkCreateShaderModule, device, &shader_module_create_info, nullptr, &shader.module);
  lifetime.tie(DeviceHandle::ShaderModule, shader.module);
  return shader;
}
