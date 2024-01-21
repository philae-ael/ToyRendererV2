#pragma once

#include <utility>
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
constexpr auto align(T offset, T aligmement) -> T {
  return (offset - 1 + aligmement) & (-aligmement);
}

struct inline_lambda {
  template <class Fn>
  auto operator+(Fn &&f) {
    const auto f_ = std::forward<Fn>(f);
    return f_();
  }
};

}  // namespace utils

// NOLINTNEXTLINE
#define INLINE_LAMBDA utils::inline_lambda{} + [&]()
