// SPDX-License-Identifier: MIT
// The noise gate against PiPedal's controls, its timing, and its double-precision definition.
// Constant-level squares make the detector's level exact, so thresholds are tested to the sample.
#include <cstring>
#include <squatch/dsp/blocks/gate.hpp>

#include "check.hpp"
#include "golden.hpp"
#include "reference.hpp"
#include "signals.hpp"

using namespace squatch::dsp;

#if __has_include("golden/gate.inc")
static const double kGolden[golden::kSegments] =
#include "golden/gate.inc"
    ;
static const double* const kGoldenRender = kGolden;
#else
static const double* const kGoldenRender = nullptr;
#endif

namespace {

constexpr double kRate = 48000.0;

struct Rig {
  Gate gate;
  Rig(float thresholdDb, float hysteresisDb, float attackMs = 1.0f, float holdMs = 10.0f, float releaseMs = 10.0f) {
    Arena none;
    gate.prepare(Setup{kRate, 128}, none);
    gate.set(Gate::Threshold, thresholdDb);
    gate.set(Gate::Hysteresis, hysteresisDb);
    gate.set(Gate::Attack, attackMs);
    gate.set(Gate::Hold, holdMs);
    gate.set(Gate::Release, releaseMs);
  }
  std::vector<float> run(const std::vector<float>& x) {
    std::vector<float> y(x.size());
    for (std::size_t i = 0; i < x.size(); i += 64) gate.process(&x[i], &y[i], std::min<int>(64, int(x.size() - i)));
    return y;
  }
};

std::vector<float> level(float db, int n) { return sig::square(96, n, dbToGain(db)); }

std::vector<float> then(std::vector<float> a, const std::vector<float>& b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

// First index from `from` where |y| / |x| reaches `gain` (within 1e-4), or -1.
int reaches(const std::vector<float>& x, const std::vector<float>& y, int from, float gain) {
  for (int i = from; i < int(x.size()); ++i)
    if (x[i] != 0.0f && std::fabs(y[i] / x[i] - gain) <= gain * 1e-4f) return i;
  return -1;
}

}  // namespace

TEST(gateControlsMatchPiPedal) {  // ToobNoiseGate.ttl, as installed on the Pi bench
  struct Want {
    const char* name;
    const char* unit;
    float min, max, def;
  } const want[] = {{"Threshold", "dB", -120, 0, -90}, {"Hysteresis", "dB", -30, 0, -6}, {"Range", "dB", -60, -6, -60},
                    {"Attack", "ms", 1, 500, 1},       {"Hold", "ms", 1, 100, 100},      {"Release", "ms", 10, 1000, 330}};
  const Span<const ParamDesc> ps = Gate::params();
  CHECK(ps.size() == 6, "six controls");
  for (std::size_t i = 0; i < ps.size() && i < 6; ++i) {
    const bool same = ps[i].id == i && !std::strcmp(ps[i].name, want[i].name) && !std::strcmp(ps[i].unit, want[i].unit) &&
                      ps[i].min == want[i].min && ps[i].max == want[i].max && ps[i].def == want[i].def;
    CHECK(same, "control %zu (%s)", i, ps[i].name);
  }
}

TEST(gateOpensAtThreshold) {
  Rig below(-40.0f, -6.0f), at(-40.0f, -6.0f);
  const std::vector<float> quiet = level(-40.1f, 9600), loud = level(-40.0f, 9600);
  const std::vector<float> yq = below.run(quiet), ya = at.run(loud);
  CHECK(!below.gate.open() && std::fabs(sig::db(yq[9000] / quiet[9000]) + 60.0) < 0.01, "0.1 dB under: closed, -60 dB");
  CHECK(at.gate.open() && ya[9000] == loud[9000], "at the threshold: open, unity");
}

TEST(gateHysteresisKeepsItOpen) {
  Rig r(-40.0f, -10.0f);
  const std::vector<float> between = level(-45.0f, 24000);  // under open, over close
  const std::vector<float> y = r.run(then(level(-30.0f, 4800), between));
  CHECK(r.gate.open() && y.back() == between.back(), "opened loud, stays open between the thresholds");
  Rig fresh(-40.0f, -10.0f);
  fresh.run(between);
  CHECK(!fresh.gate.open(), "the same level never opens a closed gate");
  r.run(level(-50.1f, 4800));
  CHECK(!r.gate.open(), "under the close threshold it closes");
}

TEST(gateHoldsForHoldTime) {
  Rig r(-40.0f, -6.0f, 1.0f, 20.0f);  // 20 ms = 960 samples
  std::vector<float> x = then(level(-20.0f, 4800), std::vector<float>(4800, 0.0f));
  Gate& g = r.gate;
  int closedAt = -1;
  for (int i = 0; i < int(x.size()); ++i) {
    float y;
    g.process(&x[i], &y, 1);
    if (!g.open() && closedAt < 0 && i > 4800) closedAt = i;
  }
  CHECK(closedAt - 4800 == 959 || closedAt - 4800 == 960, "held %d samples, want 960", closedAt - 4800);
}

TEST(gateAttackAndReleaseTimes) {
  Rig r(-40.0f, -6.0f, 10.0f, 1.0f, 100.0f);  // attack 480 samples, release 4800
  const std::vector<float> x = then(then(level(-80.0f, 4800), level(-20.0f, 9600)), level(-80.0f, 14400));
  const std::vector<float> y = r.run(x);
  const int unity = reaches(x, y, 4800, 1.0f);
  CHECK(unity - 4800 >= 479 && unity - 4800 <= 481, "floor to unity in %d samples, want 480", unity - 4800);
  const int floorAt = reaches(x, y, 14400, dbToGain(-60.0f));
  const int holdN = 48;
  CHECK(std::abs(floorAt - 14400 - holdN - 4800) <= 3, "hold, then unity to floor in %d samples, want 4800",
        floorAt - 14400 - holdN);
}

TEST(gateRangeIsTheFloor) {
  for (float range : {-60.0f, -30.0f, -6.0f}) {
    Rig r(-20.0f, -6.0f);
    r.gate.set(Gate::Range, range);
    const std::vector<float> x = level(-50.0f, 48000);
    const std::vector<float> y = r.run(x);
    CHECK(std::fabs(sig::db(y.back() / x.back()) - range) < 0.01, "closed attenuation %.3f dB, want %.0f",
          sig::db(y.back() / x.back()), range);
  }
}

TEST(gateGolden) {
  // A plucked-note shape over a noise floor: decaying 110 Hz tones separated by hiss.
  std::vector<float> x = sig::noise(65536, 0.002f, 31);
  for (int start : {4000, 24000, 44000})
    for (int i = 0; i < 16000; ++i)
      x[start + i] += float(0.5 * std::exp(-i / 4000.0) * std::sin(2 * sig::kPi * 110.0 * i / kRate));
  Rig r(-40.0f, -8.0f, 2.0f, 50.0f, 200.0f);
  r.gate.set(Gate::Range, -40.0f);
  const std::vector<float> y = r.run(x);
  const double e = sig::esr(y, ref::Gate{-40, -8, -40, 2, 50, 200, kRate}.run(x));
  CHECK(e < 1e-9, "ESR against the double-precision definition %.3g", e);
  std::printf("     gate ESR vs reference: %.3g\n", e);
  golden::check("gate", y, kGoldenRender, 1e-5);
}
