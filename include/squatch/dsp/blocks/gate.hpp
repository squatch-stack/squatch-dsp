// SPDX-License-Identifier: MIT
// Noise gate with the controls of PiPedal's (ToobAmp) noise gate, same ranges and defaults:
// threshold, hysteresis, range, attack, hold, release.
//
// The gate opens when the peak level reaches the threshold and stays open while the level is at
// or above threshold + hysteresis (hysteresis is <= 0 dB). Once the level has stayed below that
// for the hold time it closes. Gain moves in dB at a constant rate: from the range floor to unity
// in the attack time, and back down in the release time.
#pragma once
#include <squatch/dsp/block.hpp>
#include <squatch/dsp/param.hpp>

namespace squatch::dsp {

class Gate final : public Block {
 public:
  enum Param : ParamId { Threshold, Hysteresis, Range, Attack, Hold, Release, kCount };
  static Span<const ParamDesc> params();

  Gate();
  void prepare(const Setup& s, Arena& mem) override;
  void process(const float* in, float* out, int n) override;
  void reset() override;
  void set(ParamId id, float value) override;

  bool open() const { return open_; }
  float gain() const { return gain_; }

 private:
  void detect(float level);
  float step();
  void update();

  float value_[kCount];
  double sampleRate_ = 48000.0;
  float openLevel_ = 0.0f, closeLevel_ = 0.0f, floor_ = 0.0f;
  float up_ = 1.0f, down_ = 1.0f;  // per-sample gain factors
  int holdSamples_ = 0;
  int holdLeft_ = 0;
  float gain_ = 0.0f;
  bool open_ = false;
};

}  // namespace squatch::dsp
