#include "utils.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

auto tr::renderer::check_extensions(const char* kind, std::span<const char* const> required,
                                    std::span<const char* const> optional, std::span<VkExtensionProperties> available)
    -> std::optional<std::set<std::string>> {
  std::set<std::string> available_set;
  spdlog::trace("Available {} extensions", kind);
  for (const auto& extension : available) {
    spdlog::trace("\t{}", extension.extensionName);
    available_set.insert(extension.extensionName);
  }

  // Required
  const std::set<std::string> required_set{required.begin(), required.end()};
  spdlog::trace("Required {} extensions:", kind);
  for (const auto& required_extension : required) {
    spdlog::trace("\t{}", required_extension);
  }

  std::vector<std::string> missing;
  std::set_difference(required_set.begin(), required_set.end(), available_set.begin(), available_set.end(),
                      std::back_inserter(missing));

  if (!missing.empty()) {
    spdlog::warn("Missing Required {} extensions:", kind);
    for (const auto& extension : missing) {
      spdlog::warn("\t{}:", extension);
    }
    return std::nullopt;
  }

  // Optional
  const std::set<std::string> optional_set{optional.begin(), optional.end()};
  spdlog::trace("Optional {} extensions:", kind);
  for (const auto& optional_extension : optional) {
    spdlog::trace("\t{}", optional_extension);
  }
  std::vector<std::string> optional_available;
  std::set_intersection(optional_set.begin(), optional_set.end(), available_set.begin(), available_set.end(),
                        std::back_inserter(optional_available));

  if (!optional_available.empty()) {
    spdlog::trace("Got optional {} extensions:", kind);
    for (const auto& extension : optional_available) {
      spdlog::trace("\t{}:", extension);
    }
  }

  std::set<std::string> out = required_set;
  std::copy(optional_available.begin(), optional_available.end(), std::inserter(out, out.begin()));
  spdlog::trace("extensions that will be activated:", kind);
  for (const auto& extension : out) {
    spdlog::trace("\t{}:", extension);
  }
  return out;
}
