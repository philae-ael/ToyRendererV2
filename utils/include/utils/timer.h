#pragma once

#include <chrono>
#include <cstddef>
#include <ratio>

#include "utils/math.h"
namespace utils {

class Timer {
 public:
  float elapsed = 0.0;
  void start() { timer_start_point = Clock::now(); }
  void stop() {
    auto timer_end_point = Clock::now();
    duration_s duration(timer_end_point - timer_start_point);
    elapsed = duration.count();
  }

 private:
  using Clock = std::chrono::high_resolution_clock;
  using duration_s = std::chrono::duration<float, std::milli>;
  Clock::time_point timer_start_point;
};

class FilteredTimer {
 public:
  void start() { timer.start(); }
  void stop() {
    timer.stop();
    filter.update(timer.elapsed);
  }

  [[nodiscard]] auto elapsed() const -> float { return filter.state; }
  [[nodiscard]] auto elapsed_raw() const -> float { return timer.elapsed; }

 private:
  Timer timer;
  math::KalmanFilter<float> filter{
      .process_covariance = 0.1,
      .noise_covariance = 1.0,
      .state = 0.0,
      .covariance = 0.0,
  };
};

// We store the data points twice to prevent a copy of the full buffer when we
// arrive at the end of the Timeline We don't want a ring buffer bc we want to
// access the data sequencially (to draw it)
template <class T, const std::size_t N = 250>
class Timeline {
 public:
  void push(float point) {
    data[index1] = point;
    data[index2] = point;

    index1 = (index1 + 1) % data.size();
    index2 = (index2 + 1) % data.size();
  }

  // if N = 1000, index2 = 1050, history is stored continously between [50;1049)
  // thus in [index1;index1 + 1000)
  auto history() -> std::span<T> { return {&data.at(std::min(index1, index2)), N}; }

 private:
  std::size_t index1 = 0;
  std::size_t index2 = N;
  std::array<T, 2 * N> data;
};

}  // namespace utils
