#pragma once

#include <vulkan/vulkan_core.h>

#include <string_view>

#define CVAR_FLOAT(name, default_) static constexpr tr::CVarFloat name{#name, (default_)};
#define CVAR_EXTENT2D(name, default_w, default_h) \
  static constexpr tr::CVarExtent2D name{#name, {(default_w), (default_h)}};

namespace Json {
class Value;
}

namespace tr {

class Registry {
  using json = Json::Value;

 public:
  static auto global() -> json&;
  static void load();
  static void save();
  static void dump();
};

struct CVarFloat {
  const char* name;
  float default_;

  [[nodiscard]] auto resolve() const -> float;

  void save(float f) const;
};

struct CVarExtent2D {
  std::string_view name;
  VkExtent2D default_;

  [[nodiscard]] auto resolve() const -> VkExtent2D;

  void save(VkExtent2D extent) const;
  constexpr friend auto operator<=>(const CVarExtent2D& a, const CVarExtent2D& b) { return a.name <=> b.name; };
  constexpr friend auto operator==(const CVarExtent2D& a, const CVarExtent2D& b) -> bool { return a.name == b.name; }
};
}  // namespace tr
