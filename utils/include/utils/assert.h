#pragma once

#include <spdlog/spdlog.h>

#include <source_location>

#define TR_ASSERT(expr, format, ...) TR_ASSERT_INNER(expr, #expr, format, __VA_ARGS__)

#define TR_ASSERT_INNER(expr, expr_str, format, ...) \
  static_cast<bool>(expr) ? void(0) : utils::details::logger{}.fail(expr_str, format __VA_OPT__(, ) __VA_ARGS__)

namespace utils::details {
struct logger {
  std::source_location loc;
  explicit logger(const std::source_location loc = std::source_location::current()) : loc(loc) {}

  template <typename... Args>
  [[noreturn]] void fail(const char *expr, const std::format_string<Args...> fmt, Args &&...args) {
    spdlog::critical("assertion failed: {}", expr);
    spdlog::critical(fmt.get(), std::forward<Args>(args)...);
    spdlog::info("assertion occured in {}:{} in {}", loc.function_name(), loc.line(), loc.file_name());

    std::abort();
  }
};

}  // namespace utils::details
