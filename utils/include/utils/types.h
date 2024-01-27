#pragma once

#include <chrono>
#include <optional>
#include <ranges>
#include <type_traits>
namespace utils::types {

template <class T>
concept trivial = std::is_trivial_v<T>;

template <class T>
concept trivially_copiable = std::is_trivially_copyable_v<T>;

template <class T>
struct Extent2d {
  T width, height;
  auto aspect_ratio() -> float { return static_cast<float>(width) / static_cast<float>(height); }
};

template <class T>
class not_null_pointer {
 public:
  auto get() const -> const T* { return ptr_; }
  auto get() -> T* { return ptr_; }

  constexpr explicit not_null_pointer(T& t) : ptr_(&t) {}
  not_null_pointer(const not_null_pointer&) = default;
  not_null_pointer(not_null_pointer&&) = default;
  auto operator=(const not_null_pointer&) -> not_null_pointer& = default;
  auto operator=(not_null_pointer&&) -> not_null_pointer& = default;
  ~not_null_pointer() = default;

  auto operator*() -> T& { return *ptr_; }
  auto operator*() const -> const T& { return *ptr_; }

  auto operator->() -> T* { return ptr_; }
  auto operator->() const -> const T* { return ptr_; }

 private:
  T* ptr_;
};

template <class T>
not_null_pointer(T&) -> not_null_pointer<T>;

template <class T>
auto make_non_null_pointer(T* t) -> std::optional<not_null_pointer<T>> {
  if (t == nullptr) {
    return std::nullopt;
  }

  return {*t};
}

template <class T, class V>
concept range_of = std::ranges::range<T> && std::is_convertible_v<V, std::ranges::range_reference_t<T>>;

template <class T, class clock = std::chrono::steady_clock>
struct debouncer {
  clock::duration d = std::chrono::milliseconds(100);
  std::optional<std::pair<typename clock::time_point, T>> last_val = std::nullopt;

  template <class Fn1, class Fn2>
  void debounce(Fn1 fn1, Fn2 fn2) {
    std::optional<T> res = fn1();
    if (res) {
      last_val = std::pair{clock::now(), *res};
    }

    if (last_val && d <= (clock::now() - last_val->first)) {
      fn2(last_val->second);
      last_val.reset();
    }
  }
};
}  // namespace utils::types
