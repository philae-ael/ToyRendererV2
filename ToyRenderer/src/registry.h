#pragma once

#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>
#include <spdlog/spdlog.h>

#include <fstream>

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
    Json::parseFromStream(Json::CharReaderBuilder{}, i, &global(), nullptr);
  }

  static void save() {
    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = "    ";

    std::ofstream o("registry.json");
    o << Json::writeString(wbuilder, global());
    o.close();
  }
};
}  // namespace tr
