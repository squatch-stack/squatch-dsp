// SPDX-License-Identifier: MIT
// A pointer and a length (C++17 has no std::span), plus the few buffer helpers blocks share.
#pragma once
#include <cmath>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace squatch::dsp {

template <class T>
class Span {
 public:
  constexpr Span() = default;
  constexpr Span(T* data, std::size_t size) : data_(data), size_(size) {}
  template <std::size_t N>
  constexpr Span(T (&array)[N]) : data_(array), size_(N) {}
  template <class U, class = std::enable_if_t<std::is_same<const U, T>::value>>
  constexpr Span(Span<U> other) : data_(other.data()), size_(other.size()) {}

  constexpr T* data() const { return data_; }
  constexpr std::size_t size() const { return size_; }
  constexpr bool empty() const { return size_ == 0; }
  constexpr T& operator[](std::size_t i) const { return data_[i]; }
  constexpr T* begin() const { return data_; }
  constexpr T* end() const { return data_ + size_; }

 private:
  T* data_ = nullptr;
  std::size_t size_ = 0;
};

/// Copies n samples; in and out may be the same buffer or overlap.
inline void copy(const float* in, float* out, int n) {
  if (in != out && n > 0) std::memmove(out, in, sizeof(float) * static_cast<std::size_t>(n));
}

inline void clear(float* out, int n) {
  if (n > 0) std::memset(out, 0, sizeof(float) * static_cast<std::size_t>(n));
}

inline float peakAbs(const float* x, int n) {
  float peak = 0.0f;
  for (int i = 0; i < n; ++i) peak = std::fmax(peak, std::fabs(x[i]));
  return peak;
}

inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
inline float gainToDb(float gain) { return 20.0f * std::log10(gain); }

}  // namespace squatch::dsp
