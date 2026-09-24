// SPDX-License-Identifier: MIT
// The tuner behind the Block interface. Its accuracy is tested in blocks/tuner (make test runs
// that suite too); here: the reading arrives through the snapshot, audio passes or mutes.
#include <squatch/dsp/blocks/tuner.hpp>

#include "check.hpp"
#include "signals.hpp"

using namespace squatch::dsp;

namespace {

constexpr double kRate = 48000.0;

TunerBlock& prepared() {
  static TunerBlock t;  // ~128 kB: static, as a host keeps it
  Arena none;
  t.set(TunerBlock::Mute, 0.0f);
  t.prepare(Setup{kRate, 64}, none);
  return t;
}

}  // namespace

TEST(tunerBlockPublishesReadings) {
  TunerBlock& t = prepared();
  const double hz = 110.0 * std::exp2(13.0 / 1200.0);  // A2, 13 cents sharp
  const std::vector<float> x = sig::sine(hz, 28800, kRate, 0.3);
  std::vector<float> y(x.size());
  for (size_t i = 0; i < x.size(); i += 64) t.process(&x[i], &y[i], 64);
  CHECK(t.readings().update(), "a snapshot was published");
  const TunerSnapshot& s = t.readings().front();
  CHECK(s.samples == x.size(), "snapshot after %llu samples", static_cast<unsigned long long>(s.samples));
  CHECK(s.reading.valid && s.reading.midi == 45 && std::fabs(s.reading.cents - 13.0f) < 0.01f, "A2 %+.3f c (midi %d)",
        s.reading.cents, s.reading.midi);
  CHECK(!t.readings().update(), "nothing newer until the next process()");
  CHECK(y == x, "unmuted, the input passes through bit for bit");
}

TEST(tunerBlockMutesWithoutClicks) {
  TunerBlock& t = prepared();
  const std::vector<float> x = sig::sine(196.0, 4800, kRate, 0.5);
  std::vector<float> y(x.size());
  t.set(TunerBlock::Mute, 1.0f);
  t.process(x.data(), y.data(), 4800);
  float biggestStep = 0.0f;
  for (int i = 1; i < 480; ++i) biggestStep = std::fmax(biggestStep, std::fabs(y[i] - y[i - 1]));
  CHECK(biggestStep < 0.05f, "a 5 ms ramp, not a click: biggest step %f", biggestStep);
  CHECK(peakAbs(&y[240], 4800 - 240) == 0.0f, "silent once the ramp is done");
  CHECK(t.readings().update() && t.readings().front().samples == 4800, "still tuning while muted");
}
