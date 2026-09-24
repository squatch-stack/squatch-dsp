// SPDX-License-Identifier: MIT
#include <squatch/dsp/blocks/eq.hpp>

namespace squatch::dsp {
namespace {

const ParamDesc kParams[] = {
    {Eq::LowFreq, "Low freq", "Hz", 20.0f, 1000.0f, 100.0f, 20.0f},
    {Eq::LowGain, "Low gain", "dB", -18.0f, 18.0f, 0.0f, 20.0f},
    {Eq::MidFreq, "Mid freq", "Hz", 100.0f, 10000.0f, 1000.0f, 20.0f},
    {Eq::MidGain, "Mid gain", "dB", -18.0f, 18.0f, 0.0f, 20.0f},
    {Eq::MidQ, "Mid Q", "", 0.1f, 10.0f, 0.7f, 20.0f},
    {Eq::HighFreq, "High freq", "Hz", 1000.0f, 20000.0f, 5000.0f, 20.0f},
    {Eq::HighGain, "High gain", "dB", -18.0f, 18.0f, 0.0f, 20.0f},
};
static_assert(sizeof(kParams) / sizeof(kParams[0]) == Eq::kCount, "one descriptor per parameter");

}  // namespace

Span<const ParamDesc> Eq::params() { return kParams; }

Eq::Eq() {
  for (const ParamDesc& d : kParams) p_[d.id].jump(d.def);
  design();
}

void Eq::prepare(const Setup& s, Arena&) {
  sampleRate_ = s.sampleRate;
  for (const ParamDesc& d : kParams) p_[d.id].prepare(s.sampleRate, d.smoothMs);
  design();
  reset();
}

void Eq::process(const float* in, float* out, int n) {
  for (int done = 0; done < n;) {
    const int len = smoothing() ? (n - done < kControlRate ? n - done : kControlRate) : n - done;
    if (smoothing()) advance(len);
    bands_[0].process(in + done, out + done, len);
    bands_[1].process(out + done, out + done, len);
    bands_[2].process(out + done, out + done, len);
    done += len;
  }
}

void Eq::reset() {
  for (Biquad& b : bands_) b.reset();
}

void Eq::set(ParamId id, float value) {
  if (id >= kCount) return;
  p_[id].setTarget(kParams[id].clamp(value));
}

bool Eq::smoothing() const {
  for (const Smoother& s : p_)
    if (s.smoothing()) return true;
  return false;
}

// Moves the smoothers to the end of this sub-block and designs for where they land.
void Eq::advance(int n) {
  for (Smoother& s : p_) s.advance(n);
  design();
}

void Eq::design() {
  const double fs = sampleRate_;
  bands_[0].setCoeffs(designBiquad(Shape::LowShelf, fs, p_[LowFreq].value(), p_[LowGain].value(), kShelfQ));
  bands_[1].setCoeffs(designBiquad(Shape::Peak, fs, p_[MidFreq].value(), p_[MidGain].value(), p_[MidQ].value()));
  bands_[2].setCoeffs(designBiquad(Shape::HighShelf, fs, p_[HighFreq].value(), p_[HighGain].value(), kShelfQ));
}

}  // namespace squatch::dsp
