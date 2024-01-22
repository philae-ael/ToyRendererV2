#pragma once

#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <fstream>
#include <sstream>

#define CVAR_FLOAT(name, default_) static constexpr tr::CVarFloat name{#name, (default_)};
#define CVAR_EXTENT2D(name, default_w, default_h) \
  static constexpr tr::CVarExtent2D name{#name, {(default_w), (default_h)}};

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

  static void dump() {
    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = "    ";

    std::ostringstream s;
    s << Json::writeString(wbuilder, global());
    spdlog::debug(s.str());
  }
};

struct CVarFloat {
  const char* name;
  float default_;

  [[nodiscard]] auto resolve() const -> float {
    const auto f = tr::Registry::global()["cvar"][name];
    if (!f.isNumeric()) {
      save(default_);
      return default_;
    }

    return f.asFloat();
  }

  void save(float f) const { tr::Registry::global()["cvar"][name] = f; }
};

struct CVarExtent2D {
  const char* name;
  VkExtent2D default_;

  [[nodiscard]] auto resolve() const -> VkExtent2D {
    const auto vwidth = tr::Registry::global()["cvar"][name]["x"];
    const auto vheight = tr::Registry::global()["cvar"][name]["y"];
    if (!vwidth.isUInt() || !vheight.isUInt()) {
      save(default_);
      return default_;
    }

    const auto width = vwidth.asUInt();
    const auto height = vheight.asUInt();
    return VkExtent2D{width, height};
  }

  void save(VkExtent2D extent) const {
    tr::Registry::global()["cvar"][name]["x"] = extent.width;
    tr::Registry::global()["cvar"][name]["y"] = extent.height;
  }
};
}  // namespace tr
