#pragma once

#include <spdlog/spdlog.h>

#include <chrono>
#include <string_view>

// NOLINTNEXTLINE
#define INLINE_LAMBDA utils::inline_lambda{} + [&]()

#ifdef TIMED_INLINE_LAMBDA_DISABLE
// NOLINTNEXTLINE
#define TIMED_INLINE_LAMBDA(name) utils::inline_lambda{} + [&]()
#else
// NOLINTNEXTLINE
#define TIMED_INLINE_LAMBDA(name) utils::timed_inline_lambda{(name)} + [&]()
#endif

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
constexpr auto align(T offset, T aligmement) -> T {
  return (offset - 1 + aligmement) & (-aligmement);
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
struct timed_inline_lambda {
  std::string_view name;

  template <class Fn>
  auto operator+(Fn f) {
    const auto start = std::chrono::high_resolution_clock::now();
    DEFER {
      const auto end = std::chrono::high_resolution_clock::now();
      float duration_s = std::chrono::duration_cast<std::chrono::duration<float>>(end - start).count();
      spdlog::debug("{}: {:.0f}ms", name, duration_s * 1000);
    };

    return f();
  }
};

}  // namespace utils
