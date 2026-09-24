// SPDX-License-Identifier: MIT
// Lookahead true-peak limiter: ear safety at the end of every chain. The Ceiling is a true-peak
// ceiling: neither the output samples nor the waveform between them go over it.
//
// Detector, per stretch between two samples: the sample peak s and the peak i of seven points
// between them (8x polyphase Kaiser-sinc interpolation). A short interpolator reads peaks built
// from content near fs/2 low, so the estimate counts the inter-sample excess twice,
// s + 2 (i - s), and adds a fixed 0.05 dB. What each part closes, measured at 16x with a 96-tap
// meter on the test set at 44.1, 48 and 96 kHz (dB over the ceiling without it):
//   8x detector alone:     tones +0.04, squares +0.2, onsets +0.5, white noise to fs/2 +1.0
//   + excess counted twice: all closed except band-limited noise at 44.1 kHz (+0.013) and the
//                          ringing of a hard onset (+0.036)
//   + 0.05 dB:             all closed, worst -0.014
// The price: nothing for material below ~5 kHz, but a pure high tone is held below the ceiling
// by its own inter-sample excess (8 kHz: up to 0.5 dB; fs/4 at 45 degrees: 2.3 dB).
//
// Gain: the smallest gain needed over the lookahead window (a sliding minimum), an exponential
// release, then a moving average as long as the lookahead. With window lengths D+2 and D+1 for a
// delay of D, the averaged gain is never above what the sample being output needs, so the output
// reaches the ceiling without crossing it and without a discontinuity. A hard clamp at the
// ceiling follows as the last guarantee on the samples: it only engages on float rounding, after a
// ceiling cut (the lookahead holds audio gained for the old ceiling), or on non-audio input (NaN
// becomes silence, anything past +80 dBFS is treated as +80 dBFS).
#pragma once
#include <cstdint>
#include <squatch/dsp/block.hpp>
#include <squatch/dsp/param.hpp>

namespace squatch::dsp {

class Limiter final : public Block {
 public:
  enum Param : ParamId { Ceiling, Release, kCount };
  static Span<const ParamDesc> params();

  static constexpr float kLookaheadMs = 1.5f;
  static constexpr int kTaps = 32;    // per phase of the true-peak interpolator (Kaiser, beta 8)
  static constexpr int kPhases = 7;   // 1/8 to 7/8 of the way between samples
  static constexpr float kExcessWeight = 2.0f;  // inter-sample excess over the sample peak, counted twice
  static constexpr float kMarginDb = 0.05f;
  static constexpr float kMaxInput = 1e4f;

  Limiter();
  void prepare(const Setup& s, Arena& mem) override;
  void process(const float* in, float* out, int n) override;
  void reset() override;
  void set(ParamId id, float value) override;
  int latency() const override { return latency_; }

  float gain() const { return gain_; }  // the gain applied to the last output sample
  bool ready() const { return ready_; }

 private:
  float tick(float x);
  float detect(float x);
  float hold(float need);
  float smooth(float held);
  float delay(float x);
  void designInterpolator();
  void update();

  float value_[kCount];
  double sampleRate_ = 48000.0;
  float ceiling_ = 1.0f;
  float margin_ = 1.0f;
  float releaseCoef_ = 1.0f;
  float fir_[kTaps][kPhases + 1] = {};  // tap-major, padded to 8 lanes, so the phases vectorise

  int lookahead_ = 0, latency_ = 0;
  float* delay_ = nullptr;  // latency_ samples
  int delayPos_ = 0;
  float* hist_ = nullptr;  // the last kTaps inputs, stored twice so a window never wraps
  int histPos_ = 0;
  float* holdVal_ = nullptr;  // monotonic deque for the sliding minimum
  std::uint32_t* holdAt_ = nullptr;
  int holdLen_ = 0, holdHead_ = 0, holdCount_ = 0;
  float* box_ = nullptr;  // moving average
  int boxLen_ = 0, boxPos_ = 0;
  float boxSum_ = 0.0f;

  std::uint32_t now_ = 0;
  float released_ = 1.0f;
  float gain_ = 1.0f;
  bool ready_ = false;
};

}  // namespace squatch::dsp
