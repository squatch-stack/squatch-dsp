// SPDX-License-Identifier: MIT
#include <cmath>
#include <squatch/dsp/blocks/biquad.hpp>
#include <squatch/dsp/denormal.hpp>

namespace squatch::dsp {
namespace {

struct Proto {
  double g, k, m0, m1, m2;
};

// Simper's mappings; each reduces to the cookbook's H(s) (checked against it in the tests).
Proto prototype(Shape shape, double t, double A, double q) {
  const double k = 1.0 / q;
  switch (shape) {
    case Shape::LowShelf: return {t / std::sqrt(A), k, 1.0, k * (A - 1.0), A * A - 1.0};
    case Shape::HighShelf: return {t * std::sqrt(A), k, A * A, k * (1.0 - A) * A, 1.0 - A * A};
    case Shape::Peak: break;
  }
  const double kp = 1.0 / (q * A);
  return {t, kp, 1.0, kp * (A * A - 1.0), 0.0};
}

}  // namespace

BiquadCoeffs designBiquad(Shape shape, double sampleRate, double f0, double gainDb, double q) {
  const double pi = 3.14159265358979323846;
  const double f = std::fmin(std::fmax(f0, 1.0), 0.49 * sampleRate);
  const Proto p = prototype(shape, std::tan(pi * f / sampleRate), std::pow(10.0, gainDb / 40.0), std::fmax(q, 1e-3));
  const double a1 = 1.0 / (1.0 + p.g * (p.g + p.k));
  BiquadCoeffs c;
  c.g = static_cast<float>(p.g), c.k = static_cast<float>(p.k);
  c.m0 = static_cast<float>(p.m0), c.m1 = static_cast<float>(p.m1), c.m2 = static_cast<float>(p.m2);
  c.a1 = static_cast<float>(a1), c.a2 = static_cast<float>(p.g * a1), c.a3 = static_cast<float>(p.g * p.g * a1);
  return c;
}

void Biquad::process(const float* in, float* out, int n) {
  const BiquadCoeffs c = c_;
  float ic1 = ic1_, ic2 = ic2_;
  for (int i = 0; i < n; ++i) {
    const float x = in[i];
    const float v3 = x - ic2;
    const float v1 = c.a1 * ic1 + c.a2 * v3;
    const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    out[i] = c.m0 * x + c.m1 * v1 + c.m2 * v2;
  }
  ic1_ = flushDenormal(ic1);
  ic2_ = flushDenormal(ic2);
}

}  // namespace squatch::dsp
