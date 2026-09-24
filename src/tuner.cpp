// SPDX-License-Identifier: MIT
#include <squatch/dsp/blocks/tuner.hpp>

namespace squatch::dsp {
namespace {

const ParamDesc kParams[] = {
    {TunerBlock::Mute, "Mute", "", 0.0f, 1.0f, 0.0f, 5.0f},
};
static_assert(sizeof(kParams) / sizeof(kParams[0]) == TunerBlock::kCount, "one descriptor per parameter");

}  // namespace

Span<const ParamDesc> TunerBlock::params() { return kParams; }

void TunerBlock::prepare(const Setup& s, Arena&) {
  config_.sampleRate = static_cast<float>(s.sampleRate);
  tuner_.configure(config_);
  mute_.prepare(s.sampleRate, kParams[Mute].smoothMs);
  samples_ = 0;
}

void TunerBlock::process(const float* in, float* out, int n) {
  tuner_.process(in, n);  // before out is written: in may be out
  samples_ += static_cast<std::uint64_t>(n);
  TunerSnapshot& s = readings_.back();
  s.reading = tuner_.reading();
  s.samples = samples_;
  readings_.publish();
  output(in, out, n);
}

void TunerBlock::reset() {
  tuner_.reset();
  samples_ = 0;
}

void TunerBlock::set(ParamId id, float value) {
  if (id < kCount) mute_.setTarget(kParams[id].clamp(value));
}

// Through, muted, or ramping between the two.
void TunerBlock::output(const float* in, float* out, int n) {
  if (!mute_.smoothing()) {
    if (mute_.value() >= 1.0f) return clear(out, n);
    if (mute_.value() <= 0.0f) return copy(in, out, n);
  }
  for (int i = 0; i < n; ++i) out[i] = in[i] * (1.0f - mute_.next());
}

}  // namespace squatch::dsp
