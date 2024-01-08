#pragma once

namespace utils::math {
inline constexpr float PI = 3.14159265359F;
inline constexpr float PI_2 = PI / 2.F;
inline constexpr float PI_4 = PI / 4.F;

template <class T = float>
struct KalmanFilter {
  // Note: we suppose that the model is **static** and one dimensionnal

  // Parameters: they should not be null
  T process_covariance = 1.0;
  T noise_covariance = 1.0;

  T state = 0.0;
  T covariance = 0.0;

  void update(double mesured) {
    T predicted_state = state + 0;
    T predicted_covariance = covariance + process_covariance;

    T residual = mesured - predicted_state;
    T gain = predicted_covariance / (noise_covariance + predicted_covariance);

    state = predicted_state + gain * residual;
    covariance = (1 - gain) * predicted_covariance;
  }
};

}  // namespace utils::math
