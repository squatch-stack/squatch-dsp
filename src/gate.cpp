// SPDX-License-Identifier: MIT
#include <cmath>
#include <squatch/dsp/blocks/gate.hpp>

namespace squatch::dsp {
namespace {

const ParamDesc kParams[] = {
    {Gate::Threshold, "Threshold", "dB", -120.0f, 0.0f, -90.0f, 0.0f},
    {Gate::Hysteresis, "Hysteresis", "dB", -30.0f, 0.0f, -6.0f, 0.0f},
    {Gate::Range, "Range", "dB", -60.0f, -6.0f, -60.0f, 0.0f},
    {Gate::Attack, "Attack", "ms", 1.0f, 500.0f, 1.0f, 0.0f},
    {Gate::Hold, "Hold", "ms", 1.0f, 100.0f, 100.0f, 0.0f},
    {Gate::Release, "Release", "ms", 10.0f, 1000.0f, 330.0f, 0.0f},
};
static_assert(sizeof(kParams) / sizeof(kParams[0]) == Gate::kCount, "one descriptor per parameter");

float samples(float ms, double sampleRate) { return static_cast<float>(std::fmax(1.0, ms * 1e-3 * sampleRate)); }

}  // namespace

Span<const ParamDesc> Gate::params() { return kParams; }

Gate::Gate() {
  for (const ParamDesc& d : kParams) value_[d.id] = d.def;
  update();
  reset();
}

void Gate::prepare(const Setup& s, Arena&) {
  sampleRate_ = s.sampleRate;
  update();
  reset();
}

void Gate::process(const float* in, float* out, int n) {
  for (int i = 0; i < n; ++i) {
    const float x = in[i];
    detect(std::fabs(x));
    out[i] = x * step();
  }
}

void Gate::reset() {
  open_ = false;
  holdLeft_ = 0;
  gain_ = floor_;
}

void Gate::set(ParamId id, float value) {
  if (id >= kCount) return;
  value_[id] = kParams[id].clamp(value);
  update();
}

void Gate::detect(float level) {
  if (level >= closeLevel_ && (open_ || level >= openLevel_)) {
    open_ = true;
    holdLeft_ = holdSamples_;
  } else if (open_ && --holdLeft_ <= 0) {
    open_ = false;
  }
}

// Towards unity while open, towards the floor while closed; a range change moves the same way.
float Gate::step() {
  const float target = open_ ? 1.0f : floor_;
  gain_ = gain_ < target ? std::fmin(gain_ * up_, target) : std::fmax(gain_ * down_, target);
  return gain_;
}

// Parameters apply at once: the thresholds are comparisons, and the gain already ramps.
void Gate::update() {
  openLevel_ = dbToGain(value_[Threshold]);
  closeLevel_ = dbToGain(value_[Threshold] + value_[Hysteresis]);
  floor_ = dbToGain(value_[Range]);
  const float rangeDb = -value_[Range];
  up_ = dbToGain(rangeDb / samples(value_[Attack], sampleRate_));
  down_ = dbToGain(-rangeDb / samples(value_[Release], sampleRate_));
  holdSamples_ = static_cast<int>(samples(value_[Hold], sampleRate_));
}

}  // namespace squatch::dsp
