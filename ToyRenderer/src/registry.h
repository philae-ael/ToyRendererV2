#pragma once

#include <spdlog/spdlog.h>

#include <fstream>
#include <json/json.h>

namespace tr {
class Registry {
  using json = Json::Value;

 public:
  static auto global() -> json& {
    static json global_registry;
    return global_registry;
  }

  static void load() {
    std::ifstream i("registry.json");
    try {
      i >> global();
    } catch (...) {
      spdlog::error("invalid or empty json file, content will be discarded");
      global() = {};
    }
  }
  static void save() {
    std::ofstream o("registry.json");
    o << std::setw(4) << global() << std::endl;
  }
};
}  // namespace tr
