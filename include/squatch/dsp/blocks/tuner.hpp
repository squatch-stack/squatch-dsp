// SPDX-License-Identifier: MIT
// The Squatch Tuner (blocks/tuner) as a Block. A tuner makes no sound of its own: process()
// passes the input through, or mutes it as a pedal tuner mutes the rig, and publishes the newest
// reading through a lock-free triple buffer that one display thread reads.
//
// The tuner core keeps its ~128 kB of fixed buffers as members rather than taking them from the
// arena, so give this block static storage. Its note range is set with configure() before
// prepare(), which builds tables and is not real-time safe. A4 is applied by the display
// (hosts/common/display.hpp), as the live host does.
#pragma once
#include <cstdint>
#include <squatch/dsp/block.hpp>
#include <squatch/dsp/latest.hpp>
#include <squatch/dsp/param.hpp>
#include <squatch/tuner/tuner.hpp>

namespace squatch::dsp {

struct TunerSnapshot {
  tuner::TunerReading reading;
  std::uint64_t samples = 0;  // input samples seen when it was taken
};

class TunerBlock final : public Block {
 public:
  enum Param : ParamId { Mute, kCount };
  static Span<const ParamDesc> params();

  /// Setup time only; prepare() fills in the sample rate.
  void configure(const tuner::TunerConfig& c) { config_ = c; }

  void prepare(const Setup& s, Arena& mem) override;
  void process(const float* in, float* out, int n) override;
  void reset() override;
  void set(ParamId id, float value) override;

  /// For one display thread: update() is true when a newer snapshot is in front().
  Latest<TunerSnapshot>& readings() { return readings_; }

 private:
  void output(const float* in, float* out, int n);

  tuner::TunerConfig config_{};
  tuner::Tuner tuner_;
  Latest<TunerSnapshot> readings_;
  Smoother mute_;
  std::uint64_t samples_ = 0;
};

}  // namespace squatch::dsp
