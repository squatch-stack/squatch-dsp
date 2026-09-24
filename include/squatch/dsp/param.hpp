// SPDX-License-Identifier: MIT
// Parameter descriptors and the smoother blocks use to move between values without clicks.
#pragma once
#include <cmath>

#include "block.hpp"
#include "span.hpp"

namespace squatch::dsp {

struct ParamDesc {
  ParamId id;
  const char* name;
  const char* unit;  // "dB", "ms", "Hz", "" ...
  float min;
  float max;
  float def;
  float smoothMs;  // 0: the value applies at once

  /// Inside [min, max]; NaN becomes the default.
  float clamp(float v) const { return v == v ? std::fmin(std::fmax(v, min), max) : def; }
};

inline const ParamDesc* findParam(Span<const ParamDesc> params, ParamId id) {
  for (const ParamDesc& p : params)
    if (p.id == id) return &p;
  return nullptr;
}

/// Linear ramp to the latest target over a fixed time. Linear rather than one-pole because it
/// arrives exactly, so a block knows when it can stop recomputing coefficients.
class Smoother {
 public:
  void prepare(double sampleRate, float ms) {
    steps_ = static_cast<int>(std::lround(ms * 1e-3 * sampleRate));
    jump(target_);
  }

  void jump(float v) {
    value_ = target_ = v;
    left_ = 0;
  }

  void setTarget(float v) {
    if (v == target_) return;
    if (steps_ <= 0) return jump(v);
    target_ = v;
    left_ = steps_;
    step_ = (target_ - value_) / static_cast<float>(steps_);
  }

  float next() {
    if (left_ > 0) value_ = --left_ == 0 ? target_ : value_ + step_;
    return value_;
  }

  /// Moves n samples along at once, for blocks that update at a control rate.
  float advance(int n) {
    if (left_ <= n) {
      jump(target_);
      return value_;
    }
    left_ -= n;
    value_ += step_ * static_cast<float>(n);
    return value_;
  }

  bool smoothing() const { return left_ > 0; }
  float value() const { return value_; }
  float target() const { return target_; }

 private:
  float value_ = 0.0f;
  float target_ = 0.0f;
  float step_ = 0.0f;
  int left_ = 0;
  int steps_ = 0;
};

}  // namespace squatch::dsp
