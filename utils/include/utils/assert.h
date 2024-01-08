#pragma once

#include <spdlog/spdlog.h>

#define TR_ASSERT(expr, format, ...) \
  TR_ASSERT_INNER(expr, #expr, format, __VA_ARGS__)

#define TR_ASSERT_INNER(expr, expr_str, format, ...)            \
  static_cast<bool>(expr)                                       \
      ? void(0)                                                 \
      : utils::assert_fail(expr_str, __LINE__, __FILE__,        \
                           static_cast<const char *>(__func__), \
                           format __VA_OPT__(, ) __VA_ARGS__)

namespace utils {
template <typename... Args>
[[noreturn]] void assert_fail(const char *expr, unsigned int line,
                              const char *file, const char *func,
                              spdlog::format_string_t<Args...> fmt,
                              Args &&...args) {
  spdlog::critical("assertion failed: {}", expr);
  spdlog::critical(fmt, std::forward<Args>(args)...);
  spdlog::info("assertion occured in {}:{} in {}", func, line, file);

  std::abort();
}
}  // namespace utils
