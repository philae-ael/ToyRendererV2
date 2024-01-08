#pragma once

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <source_location>

#define FORMAT_STRING_EXPOSED __cpp_lib_format >= 202207L || __GLIBCXX__ >= 20230801
#define SPDLOG_KNOWS_FORMAT_STRING_EXPOSED __cpp_lib_format >= 202207L

#if FORMAT_STRING_EXPOSED
#include <format>
#else
#include <string_view>
#endif

#define TR_ASSERT(expr, format, ...) TR_ASSERT_INNER(expr, #expr, format, __VA_ARGS__)

#define TR_ASSERT_INNER(expr, expr_str, format, ...) \
  static_cast<bool>(expr) ? void(0) : utils::details::logger{}.fail(expr_str, format __VA_OPT__(, ) __VA_ARGS__)

namespace utils::details {

// gcc supports exposing format_string but does not set __cpp_lib_format for 202207L
// Spdlog only check for __cpp_lib_format so does not use format_string.
// I want compile time help from the compiler. That explains the mess that follows
// (Or i could do a PR to spdlog and wait 3 months until i can use the fix)
//
// See
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2508r0.html
// gcc.gnu.org/onlinedocs/libstdc++/manual/status.html

template <typename... Args>
#if FORMAT_STRING_EXPOSED
using format_string = std::format_string<Args...>;
#else
using format_string = std::string_view;
#endif

template <typename... Args>
constexpr auto to_spdlog_format_string(format_string<Args...> fmt) {
#if SPDLOG_KNOWS_FORMAT_STRING_EXPOSED
  return fmt;
#elif FORMAT_STRING_EXPOSED
  return fmt.get();
#else
  return fmt;
#endif
}

struct logger {
  std::source_location loc;
  explicit logger(const std::source_location loc = std::source_location::current()) : loc(loc) {}

  template <typename... Args>
  [[noreturn]] void fail(const char *expr, format_string<Args...> fmt, Args &&...args) {
    spdlog::critical("assertion failed: {}", expr);
    spdlog::critical(to_spdlog_format_string<Args...>(fmt), std::forward<Args>(args)...);
    spdlog::info("assertion occured in {}:{} in {}", loc.function_name(), loc.line(), loc.file_name());

    std::abort();
  }
};

}  // namespace utils::details
