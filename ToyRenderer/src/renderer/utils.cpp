#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <span>
#include "utils.h"

auto tr::renderer::check_extensions(const char* kind, std::span<const char* const> required,
                                    std::span<VkExtensionProperties> available_extensions) -> bool {
  std::set<std::string> available_set;
  spdlog::debug("Available {} extensions", kind);
  for (const auto& extension : available_extensions) {
    spdlog::debug("\t{}", extension.extensionName);
    available_set.insert(extension.extensionName);
  }

  std::set<std::string> required_set{required.begin(), required.end()};
  spdlog::debug("Required {} extensions:", kind);
  for (const auto& required_extension : required) {
    spdlog::debug("\t{}", required_extension);
  }

  std::vector<std::string> out;
  std::set_difference(required_set.begin(), required_set.end(), available_set.begin(), available_set.end(),
                      std::back_inserter(out));

  spdlog::debug("Missing {} extensions:", kind);
  if (!out.empty()) {
    for (const auto& missing : out) {
      spdlog::trace("\t{}:", missing);
    }
    return false;
  }
  return true;
}
