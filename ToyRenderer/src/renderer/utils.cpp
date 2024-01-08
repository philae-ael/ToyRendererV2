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
  spdlog::debug("Available {} extensions", kind);
  for (const auto& extension : available) {
    spdlog::debug("\t{}", extension.extensionName);
    available_set.insert(extension.extensionName);
  }

  // Required
  std::set<std::string> required_set{required.begin(), required.end()};
  spdlog::debug("Required {} extensions:", kind);
  for (const auto& required_extension : required) {
    spdlog::debug("\t{}", required_extension);
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
  std::set<std::string> optional_set{optional.begin(), optional.end()};
  spdlog::debug("Optional {} extensions:", kind);
  for (const auto& optional_extension : optional) {
    spdlog::debug("\t{}", optional_extension);
  }
  std::vector<std::string> optional_available;
  std::set_intersection(optional_set.begin(), optional_set.end(), available_set.begin(), available_set.end(),
                        std::back_inserter(optional_available));

  if (!optional_available.empty()) {
    spdlog::debug("Got optional {} extensions:", kind);
    for (const auto& extension : optional_available) {
      spdlog::debug("\t{}:", extension);
    }
  }

  std::set<std::string> out = required_set;
  std::copy(optional_available.begin(), optional_available.end(), std::inserter(out, out.begin()));
  spdlog::debug("extensions that will be activated:", kind);
  for (const auto& extension : out) {
    spdlog::debug("\t{}:", extension);
  }
  return out;
}
