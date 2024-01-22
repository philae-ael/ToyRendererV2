#pragma once

#include <format>
#include <source_location>

[[noreturn]] void assert_violation_handler(const char* expr, const std::string& s,
                                           std::source_location loc = std::source_location::current());

#define TR_ASSERT(expr, format, ...) TR_ASSERT_INNER(expr, #expr, format, __VA_ARGS__)

#define TR_ASSERT_INNER(expr, expr_str, fmt, ...) \
  static_cast<bool>(expr) ? void(0) : assert_violation_handler(expr_str, std::format(fmt __VA_OPT__(, ) __VA_ARGS__))
