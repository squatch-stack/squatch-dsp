// SPDX-License-Identifier: MIT
// One second-order section with the RBJ Audio EQ Cookbook responses (low shelf, peak, high
// shelf), realised as a trapezoidal state-variable filter (Andrew Simper, Cytomic). The transfer
// function is the cookbook's exactly; the structure is not direct form because float direct-form
// coefficients cannot place poles near DC (measured: a 20 Hz shelf off by 0.4 dB at 96 kHz), while
// the SVF's g = tan(pi f0 / fs) and k = 1/Q keep full relative precision. It also modulates well.
//
// Analog prototype: H(s) = (m0 s^2 + (m0 k + m1) s + (m0 + m2)) / (s^2 + k s + 1), with
// s = j tan(pi f / fs) / g.
#pragma once

namespace squatch::dsp {

enum class Shape { LowShelf, Peak, HighShelf };

struct BiquadCoeffs {
  float g = 0.0f, k = 1.0f;                // prototype: prewarped frequency, damping
  float m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;   // output mix of input, band and low
  float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;   // derived from g and k
};

/// Designed in double. Shelves take Q as the cookbook does (0.7071: steepest with no overshoot).
/// f0 is kept below 0.49 fs.
BiquadCoeffs designBiquad(Shape shape, double sampleRate, double f0, double gainDb, double q);

class Biquad {
 public:
  void setCoeffs(const BiquadCoeffs& c) { c_ = c; }
  const BiquadCoeffs& coeffs() const { return c_; }
  void reset() { ic1_ = ic2_ = 0.0f; }
  /// In place is fine.
  void process(const float* in, float* out, int n);

 private:
  BiquadCoeffs c_;
  float ic1_ = 0.0f, ic2_ = 0.0f;
};

}  // namespace squatch::dsp
