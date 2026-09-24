// SPDX-License-Identifier: MIT
#include <cmath>
#include <squatch/dsp/blocks/limiter.hpp>

namespace squatch::dsp {
namespace {

const ParamDesc kParams[] = {
    {Limiter::Ceiling, "Ceiling", "dB", -24.0f, 0.0f, -1.0f, 0.0f},
    {Limiter::Release, "Release", "ms", 10.0f, 1000.0f, 50.0f, 0.0f},
};
static_assert(sizeof(kParams) / sizeof(kParams[0]) == Limiter::kCount, "one descriptor per parameter");

// NaN is not audio: silence. Huge values are clamped so the detector's sums stay finite.
float sanitize(float x) {
  return x == x ? std::fmin(std::fmax(x, -Limiter::kMaxInput), Limiter::kMaxInput) : 0.0f;
}

// Bessel I0 by its power series: enough terms for beta up to about 10.
double besselI0(double x) {
  double sum = 1.0, term = 1.0;
  for (int k = 1; k < 30; ++k) {
    term *= x * x / (4.0 * k * k);
    sum += term;
  }
  return sum;
}

double kaiser(double z, double beta) { return besselI0(beta * std::sqrt(std::fmax(0.0, 1.0 - z * z))) / besselI0(beta); }

}  // namespace

Span<const ParamDesc> Limiter::params() { return kParams; }

Limiter::Limiter() {
  for (const ParamDesc& d : kParams) value_[d.id] = d.def;
  designInterpolator();
  update();
}

void Limiter::prepare(const Setup& s, Arena& mem) {
  sampleRate_ = s.sampleRate;
  lookahead_ = static_cast<int>(std::ceil(kLookaheadMs * 1e-3 * s.sampleRate));
  latency_ = lookahead_ + kTaps / 2;
  holdLen_ = lookahead_ + 2;
  boxLen_ = lookahead_ + 1;
  delay_ = mem.alloc<float>(static_cast<std::size_t>(latency_));
  hist_ = mem.alloc<float>(2 * kTaps);
  holdVal_ = mem.alloc<float>(static_cast<std::size_t>(holdLen_));
  holdAt_ = mem.alloc<std::uint32_t>(static_cast<std::size_t>(holdLen_));
  box_ = mem.alloc<float>(static_cast<std::size_t>(boxLen_));
  ready_ = delay_ && hist_ && holdVal_ && holdAt_ && box_;
  update();
  reset();
}

void Limiter::process(const float* in, float* out, int n) {
  if (!ready_) return clear(out, n);
  for (int i = 0; i < n; ++i) out[i] = tick(in[i]);
}

void Limiter::reset() {
  if (!ready_) return;
  clear(delay_, latency_);
  clear(hist_, 2 * kTaps);
  delayPos_ = histPos_ = holdHead_ = holdCount_ = boxPos_ = 0;
  for (int i = 0; i < boxLen_; ++i) box_[i] = 1.0f;
  boxSum_ = static_cast<float>(boxLen_);
  now_ = 0;
  released_ = gain_ = 1.0f;
}

void Limiter::set(ParamId id, float value) {
  if (id >= kCount) return;
  value_[id] = kParams[id].clamp(value);
  update();
}

float Limiter::tick(float x) {
  const float clean = sanitize(x);
  const float peak = detect(clean);
  const float need = peak > ceiling_ ? ceiling_ / peak : 1.0f;
  gain_ = smooth(hold(need));
  ++now_;
  const float y = delay(clean) * gain_;
  return std::fmin(std::fmax(y, -ceiling_), ceiling_);
}

// The estimated true peak of the stretch between the sample kTaps/2 back and the one after it:
// the sample peak plus twice the excess of the interpolated points over it, plus the margin.
float Limiter::detect(float x) {
  hist_[histPos_] = hist_[histPos_ + kTaps] = x;
  histPos_ = histPos_ + 1 == kTaps ? 0 : histPos_ + 1;
  const float* w = hist_ + histPos_;  // oldest first
  const float samplePeak = std::fmax(std::fabs(w[kTaps / 2 - 1]), std::fabs(w[kTaps / 2]));
  float acc[kPhases + 1] = {};  // the padding lane has zero taps and stays 0
  for (int k = 0; k < kTaps; ++k)
    for (int p = 0; p <= kPhases; ++p) acc[p] += w[k] * fir_[k][p];
  float between = 0.0f;
  for (float a : acc) between = std::fmax(between, std::fabs(a));
  return (samplePeak + kExcessWeight * std::fmax(between - samplePeak, 0.0f)) * margin_;
}

// Sliding minimum over the last holdLen_ values (monotonic deque in a ring).
float Limiter::hold(float need) {
  if (holdCount_ > 0 && now_ - holdAt_[holdHead_] >= static_cast<std::uint32_t>(holdLen_)) {
    holdHead_ = holdHead_ + 1 == holdLen_ ? 0 : holdHead_ + 1;
    --holdCount_;
  }
  auto slot = [this](int i) { return holdHead_ + i >= holdLen_ ? holdHead_ + i - holdLen_ : holdHead_ + i; };
  while (holdCount_ > 0 && holdVal_[slot(holdCount_ - 1)] >= need) --holdCount_;
  const int back = slot(holdCount_++);
  holdVal_[back] = need;
  holdAt_[back] = now_;
  return holdVal_[holdHead_];
}

// Instant attack to the held gain, exponential release towards it, then the moving average.
// The sum is recomputed once per lap of the ring so rounding cannot accumulate.
float Limiter::smooth(float held) {
  released_ = held < released_ ? held : released_ + (held - released_) * releaseCoef_;
  boxSum_ += released_ - box_[boxPos_];
  box_[boxPos_] = released_;
  if (++boxPos_ == boxLen_) {
    boxPos_ = 0;
    boxSum_ = 0.0f;
    for (int i = 0; i < boxLen_; ++i) boxSum_ += box_[i];
  }
  return boxSum_ / static_cast<float>(boxLen_);  // a division, so all-ones is exactly 1
}

float Limiter::delay(float x) {
  const float y = delay_[delayPos_];
  delay_[delayPos_] = x;
  delayPos_ = delayPos_ + 1 == latency_ ? 0 : delayPos_ + 1;
  return y;
}

// Kaiser-windowed sinc (beta 8), one row per fractional position, each normalised to unity
// gain at DC.
void Limiter::designInterpolator() {
  const double pi = 3.14159265358979323846, half = kTaps / 2;
  for (int p = 0; p < kPhases; ++p) {
    const double frac = (p + 1) / double(kPhases + 1);
    double sum = 0.0, h[kTaps];
    for (int k = 0; k < kTaps; ++k) {
      const double u = frac - (k - (half - 1));
      const double sinc = u == 0.0 ? 1.0 : std::sin(pi * u) / (pi * u);
      h[k] = sinc * kaiser(u / half, 8.0);
      sum += h[k];
    }
    for (int k = 0; k < kTaps; ++k) fir_[k][p] = static_cast<float>(h[k] / sum);
  }
}

void Limiter::update() {
  ceiling_ = dbToGain(value_[Ceiling]);
  margin_ = dbToGain(kMarginDb);
  const double samples = value_[Release] * 1e-3 * sampleRate_;
  releaseCoef_ = static_cast<float>(1.0 - std::exp(-1.0 / samples));
}

}  // namespace squatch::dsp
