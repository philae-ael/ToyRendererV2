#pragma once

#include <optional>
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

struct threadsafe {};
struct threadunsafe {};

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

template<class T>
not_null_pointer(T&) -> not_null_pointer<T>;

template <class T>
auto make_non_null_pointer(T* t) -> std::optional<not_null_pointer<T>> {
  if (t == nullptr) {
    return std::nullopt;
  }

  return {*t};
}

}  // namespace utils::types
