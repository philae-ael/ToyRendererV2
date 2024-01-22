#include "pipeline.h"

#include <shaderc/shaderc.h>
#include <shaderc/status.h>
#include <spdlog/spdlog.h>
#include <utils/asset.h>
#include <utils/cast.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <shaderc/shaderc.hpp>
#include <vector>

#include "utils.h"
#include "utils/assert.h"

auto tr::renderer::Shader::compile(shaderc::Compiler& compiler, shaderc_shader_kind kind,
                                   const shaderc::CompileOptions& options, const std::filesystem::path& path)
    -> std::optional<std::vector<uint32_t>> {
  const auto data = read_file<char>(path.string());
  if (!data) {
    return std::nullopt;
  }

  TR_ASSERT(compiler.IsValid(), "compiler is invalid");

  const auto result =
      compiler.CompileGlslToSpv(data->data(), data->size(), kind, path.filename().string().c_str(), "main", options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    spdlog::error("Shader error:\n{}", result.GetErrorMessage());
    return std::nullopt;
  }
  return std::vector<uint32_t>{result.begin(), result.end()};
}

auto tr::renderer::Shader::init_from_spv(Lifetime& lifetime, VkDevice device, std::span<const uint32_t> module)
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
