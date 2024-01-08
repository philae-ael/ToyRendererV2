#pragma once

#include <span>
#include <utility>
namespace utils {

template <class T, class U>
T narrow_cast(U&& u) {
  return static_cast<T>(std::forward<U>(u));
}

template <class T>
T* array_decay_cast(std::span<T> s) {
  return s.data();
}
}  // namespace utils
