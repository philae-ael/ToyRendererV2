#pragma once

#include <cstddef>

// NOLINTNEXTLINE
#define INLINE_LAMBDA utils::inline_lambda{} + [&]()

// NOLINTNEXTLINE
#define DEFER const auto TR_DEFER_ = utils::make_defer{} + [&]() -> void
namespace utils {

template <class... T>
void ignore_unused(const T &.../*unused*/) {}

template <typename... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// TODO: put that in a memory.h file or smthg like that
// There is std::align but it deal with pointers
template <class T>
constexpr auto align(T offset, T alignement) -> T {
  const auto alignement_ = static_cast<std::size_t>(alignement);
  return static_cast<T>((static_cast<std::size_t>(offset) - 1 + alignement_) & -alignement_);
}

template <class Fn>
struct defer : Fn {  // NOLINT
  using Fn::operator();
  ~defer() { (*this)(); }
};

struct make_defer {  // NOLINT
  template <class Fn>
  auto operator+(Fn f) -> defer<Fn> {
    return {f};
  }
};

struct inline_lambda {
  template <class Fn>
  auto operator+(Fn f) {
    return f();
  }
};

}  // namespace utils
