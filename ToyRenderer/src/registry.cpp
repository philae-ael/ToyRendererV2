#include "registry.h"

#include <json/reader.h>
#include <json/writer.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>

auto tr::Registry::global() -> json& {
  static json global_registry;
  return global_registry;
}

void tr::Registry::load() {
  std::ifstream i("registry.json");
  Json::parseFromStream(Json::CharReaderBuilder{}, i, &global(), nullptr);
}
void tr::Registry::save() {
  Json::StreamWriterBuilder wbuilder;
  wbuilder["indentation"] = "    ";

  std::ofstream o("registry.json");
  o << Json::writeString(wbuilder, global());
  o.close();
}
void tr::Registry::dump() {
  Json::StreamWriterBuilder wbuilder;
  wbuilder["indentation"] = "    ";

  std::ostringstream s;
  s << Json::writeString(wbuilder, global());
  spdlog::debug(s.str());
}
auto tr::CVarFloat::resolve() const -> float {
  const auto f = tr::Registry::global()["cvar"][name];
  if (!f.isNumeric()) {
    save(default_);
    return default_;
  }

  return f.asFloat();
}

void tr::CVarFloat::save(float f) const { tr::Registry::global()["cvar"][name] = f; }

auto tr::CVarExtent2D::resolve() const -> VkExtent2D {
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

void tr::CVarExtent2D::save(VkExtent2D extent) const {
  std::string n{name};
  tr::Registry::global()["cvar"][n]["x"] = extent.width;
  tr::Registry::global()["cvar"][n]["y"] = extent.height;
}
