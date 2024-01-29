#pragma once

#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <fstream>
#include <sstream>
#include <string_view>

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
  std::string_view name;
  VkExtent2D default_;

  [[nodiscard]] auto resolve() const -> VkExtent2D {
    std::string n{name};
    const auto vwidth = tr::Registry::global()["cvar"][n]["x"];
    const auto vheight = tr::Registry::global()["cvar"][n]["y"];
    if (!vwidth.isUInt() || !vheight.isUInt()) {
      save(default_);
      return default_;
    }

    const auto width = vwidth.asUInt();
    const auto height = vheight.asUInt();
    return VkExtent2D{width, height};
  }

  void save(VkExtent2D extent) const {
    std::string n{name};
    tr::Registry::global()["cvar"][n]["x"] = extent.width;
    tr::Registry::global()["cvar"][n]["y"] = extent.height;
  }
  constexpr friend auto operator<=>(const CVarExtent2D& a, const CVarExtent2D& b) { return a.name <=> b.name; };
  constexpr friend auto operator==(const CVarExtent2D& a, const CVarExtent2D& b) -> bool { return a.name == b.name; }
};
}  // namespace tr
