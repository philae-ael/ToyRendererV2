#pragma once

#include <type_traits>
namespace utils::types {

template <class T>
concept trivial = std::is_trivial_v<T>;

template <class T>
concept trivially_copiable = std::is_trivially_copyable_v<T>;

template <class T>
struct Extent2d {
  T width, height;
};
}  // namespace utils::types
