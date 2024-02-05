#include "utils/timer.h"

#include <spdlog/spdlog.h>

utils::timed_inline_lambda::~timed_inline_lambda() { spdlog::debug("{}: {:.0f}ms", name, duration_s * 1000); }
