#include "pipeline.h"

#include <shaderc/shaderc.h>     // for shaderc_include_result, shaderc_incl...
#include <shaderc/status.h>      // for shaderc_compilation_status_success
#include <spdlog/spdlog.h>       // for error
#include <utils/asset.h>         // for read_file
#include <utils/cast.h>          // for narrow_cast
#include <vulkan/vulkan_core.h>  // for vkCreateShaderModule, VkStructureType

#include <cstdint>              // for uint32_t
#include <cstring>              // for strlen
#include <filesystem>           // for path, operator/
#include <ios>                  // for filebuf, ios_base
#include <optional>             // for optional, nullopt
#include <shaderc/shaderc.hpp>  // for Compiler, CompileOptions (ptr only)
#include <string_view>
#include <vector>  // for vector

#include "deletion_stack.h"  // for DeviceHandle, Lifetime
#include "utils.h"           // for VK_UNWRAP
#include "utils/assert.h"    // for TR_ASSERT
#include "vkformat.h"        // IWYU pragma: keep

auto tr::renderer::Shader::compile(shaderc::Compiler& compiler, shaderc_shader_kind kind,
                                   const shaderc::CompileOptions& options, std::string_view path)
    -> std::optional<std::vector<uint32_t>> {
  const auto data = read_file<char>(path);
  if (!data) {
    return std::nullopt;
  }

  TR_ASSERT(compiler.IsValid(), "compiler is invalid");
  std::filesystem::path path_ = path;

  const auto result =
      compiler.CompileGlslToSpv(data->data(), data->size(), kind, path_.filename().string().c_str(), "main", options);

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

// NOLINTBEGIN
auto MakeErrorIncludeResult(const char* message) -> shaderc_include_result* {
  return new shaderc_include_result{"", 0, message, strlen(message), nullptr};
}

auto tr::renderer::FileIncluder::GetInclude(const char* requested_source, shaderc_include_type include_type,
                                            const char* requesting_source, size_t /*include_depth*/)
    -> shaderc_include_result* {
  const std::string full_path = (include_type == shaderc_include_type_relative)
                                    ? FindRelativeReadableFilepath(requesting_source, requested_source)
                                    : FindReadableFilepath(requested_source);

  if (full_path.empty()) {
    return MakeErrorIncludeResult("Cannot find or open include file.");
  }

  auto* new_file_info = new FileInfo{full_path, {}};
  auto contents = read_file<char>(full_path);
  if (!contents) {
    return MakeErrorIncludeResult("Cannot read file");
  }

  new_file_info->contents = std::move(*contents);
  return new shaderc_include_result{new_file_info->full_path.data(), new_file_info->full_path.length(),
                                    new_file_info->contents.data(), new_file_info->contents.size(), new_file_info};
}

void tr::renderer::FileIncluder::ReleaseInclude(shaderc_include_result* include_result) {
  auto* info = static_cast<FileInfo*>(include_result->user_data);
  delete info;
  delete include_result;
}

auto tr::renderer::FileIncluder::FindRelativeReadableFilepath(const std::string& requesting_file,
                                                              const std::string& filename) const -> std::string {
  assert(!filename.empty());

  std::string relative_filename = std::filesystem::path(requesting_file).parent_path() / filename;
  if (std::filebuf{}.open(relative_filename, std::ios_base::in) != nullptr) {
    return relative_filename;
  }

  return FindReadableFilepath(filename);
}

auto tr::renderer::FileIncluder::FindReadableFilepath(const std::string& filename) const -> std::string {
  assert(!filename.empty());
  static const auto for_reading = std::ios_base::in;
  std::filebuf opener;

  std::string prefixed_filename = std::filesystem::path{base_path} / filename;
  if (std::filebuf{}.open(prefixed_filename, for_reading) != nullptr) {
    return prefixed_filename;
  }
  return "";
}
// NOLINTEND
