// SPDX-License-Identifier: MIT
// Deterministic test signals and the measures the tests share.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

namespace sig {

constexpr double kPi = 3.14159265358979323846;

/// xorshift32: the same numbers on every platform, unlike <random>'s distributions.
struct Rng {
  std::uint32_t s = 0x9e3779b9u;
  std::uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  float uniform() { return static_cast<float>(next() >> 8) * (2.0f / 16777216.0f) - 1.0f; }  // [-1, 1)
  int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<std::uint32_t>(hi - lo + 1)); }
};

inline std::vector<float> sine(double hz, int n, double rate, double amp, double phase = 0.0) {
  std::vector<float> x(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) x[i] = static_cast<float>(amp * std::sin(2.0 * kPi * hz * i / rate + phase));
  return x;
}

/// A square wave of `period` samples (high for the first half), sample values exactly +-amp.
inline std::vector<float> square(int period, int n, float amp) {
  std::vector<float> x(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) x[i] = (i % period) * 2 < period ? amp : -amp;
  return x;
}

inline std::vector<float> noise(int n, float amp, std::uint32_t seed) {
  Rng r{seed};
  std::vector<float> x(static_cast<std::size_t>(n));
  for (float& v : x) v = amp * r.uniform();
  return x;
}

inline double db(double gain) { return 20.0 * std::log10(gain); }

inline float peak(const std::vector<float>& x) {
  float p = 0.0f;
  for (float v : x) p = std::fmax(p, std::fabs(v));
  return p;
}

/// Error-to-signal ratio: sum (y - ref)^2 / sum ref^2.
inline double esr(const std::vector<float>& y, const std::vector<double>& ref) {
  double num = 0.0, den = 0.0;
  for (std::size_t i = 0; i < y.size() && i < ref.size(); ++i) {
    num += (y[i] - ref[i]) * (y[i] - ref[i]);
    den += ref[i] * ref[i];
  }
  return den > 0.0 ? num / den : num;
}

/// Amplitude of the `hz` component, by least squares against sin and cos over x[from, to).
inline double amplitudeAt(const std::vector<float>& x, double hz, double rate, int from, int to) {
  double ss = 0, cc = 0, sc = 0, xs = 0, xc = 0;
  for (int i = from; i < to; ++i) {
    const double w = 2.0 * kPi * hz * i / rate, s = std::sin(w), c = std::cos(w);
    ss += s * s, cc += c * c, sc += s * c, xs += x[i] * s, xc += x[i] * c;
  }
  const double det = ss * cc - sc * sc;
  const double a = (xs * cc - xc * sc) / det, b = (xc * ss - xs * sc) / det;
  return std::sqrt(a * a + b * b);
}

}  // namespace sig
