// SPDX-License-Identifier: MIT
// Three-band EQ: RBJ cookbook low shelf, peak and high shelf in series (see biquad.hpp). Parameter moves are smoothed and
// the coefficients follow at a 16-sample control rate while any smoother is moving.
#pragma once
#include <squatch/dsp/block.hpp>
#include <squatch/dsp/blocks/biquad.hpp>
#include <squatch/dsp/param.hpp>

namespace squatch::dsp {

class Eq final : public Block {
 public:
  enum Param : ParamId { LowFreq, LowGain, MidFreq, MidGain, MidQ, HighFreq, HighGain, kCount };
  static Span<const ParamDesc> params();
  static constexpr float kShelfQ = 0.70710678f;
  static constexpr int kControlRate = 16;  // samples between coefficient updates while smoothing

  Eq();
  void prepare(const Setup& s, Arena& mem) override;
  void process(const float* in, float* out, int n) override;
  void reset() override;
  void set(ParamId id, float value) override;

  const Biquad& band(int i) const { return bands_[i]; }

 private:
  bool smoothing() const;
  void advance(int n);
  void design();

  Smoother p_[kCount];
  Biquad bands_[3];
  double sampleRate_ = 48000.0;
};

}  // namespace squatch::dsp
