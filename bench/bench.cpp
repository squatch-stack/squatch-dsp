// SPDX-License-Identifier: MIT
// CPU cost of each block on this machine: nanoseconds per sample at 48 kHz, for block sizes
// 16 to 128, as Markdown. Each figure is the best of 7 runs of 10 s of audio (least disturbed
// by the rest of the machine), with flush-to-zero on, as a host would run it.
#include <chrono>
#include <cstdio>
#include <ctime>
#include <squatch/dsp/blocks/eq.hpp>
#include <squatch/dsp/blocks/gate.hpp>
#include <squatch/dsp/blocks/limiter.hpp>
#include <squatch/dsp/blocks/tuner.hpp>
#include <squatch/dsp/chain.hpp>
#include <squatch/dsp/denormal.hpp>
#include <vector>

using namespace squatch::dsp;

namespace {

constexpr double kRate = 48000.0;
constexpr int kSizes[] = {16, 32, 64, 128};
alignas(64) unsigned char gMem[1 << 16];
volatile float gSink;

// Noise and a 110 Hz tone at about +6 dBFS peak: the gate is open and the limiter working.
std::vector<float> input() {
  std::vector<float> x(48000);
  unsigned s = 1;
  for (std::size_t i = 0; i < x.size(); ++i) {
    s = s * 1664525u + 1013904223u;
    x[i] = 1.5f * std::sin(6.2831853f * 110.0f * float(i) / 48000.0f) + 0.5f * (float(s >> 8) / 8388608.0f - 1.0f);
  }
  return x;
}

double timeOnce(Block& b, const std::vector<float>& x, int size) {
  static float out[128];
  const int total = int(kRate) * 10;
  const auto start = std::chrono::steady_clock::now();
  for (int done = 0; done < total; done += size) {
    const int at = done % (int(x.size()) - size);
    b.process(&x[at], out, size);
    gSink = out[size - 1];
  }
  const std::chrono::duration<double, std::nano> ns = std::chrono::steady_clock::now() - start;
  return ns.count() / total;
}

double nsPerSample(Block& b, const std::vector<float>& x, int size) {
  Arena mem(gMem, sizeof gMem);
  b.prepare(Setup{kRate, size}, mem);
  DenormalGuard ftz;
  timeOnce(b, x, size);  // warm up
  double best = 1e30;
  for (int run = 0; run < 7; ++run) best = std::min(best, timeOnce(b, x, size));
  return best;
}

// The three blocks in series, run as one Block so the same timing loop applies.
struct Series final : Block {
  Gate gate;
  Eq eq;
  Limiter limiter;
  void prepare(const Setup& s, Arena& mem) override {
    gate.prepare(s, mem), eq.prepare(s, mem), limiter.prepare(s, mem);
  }
  void process(const float* in, float* out, int n) override {
    gate.process(in, out, n), eq.process(out, out, n), limiter.process(out, out, n);
  }
  void reset() override {}
  void set(ParamId, float) override {}
};

void configure(Gate& g, Eq& e, Limiter& l) {
  g.set(Gate::Threshold, -50.0f);
  e.set(Eq::LowGain, 4.0f), e.set(Eq::MidGain, -3.0f), e.set(Eq::HighGain, 2.0f);
  l.set(Limiter::Ceiling, -1.0f);
}

void row(const char* name, Block& b, const std::vector<float>& x) {
  std::printf("| %s |", name);
  for (int size : kSizes) std::printf(" %.2f |", nsPerSample(b, x, size));
  std::printf(" %.2f%% |\n", nsPerSample(b, x, 128) * kRate / 1e7);
}

}  // namespace

int main(int argc, char** argv) {
  static Gate gate;
  static Eq eq;
  static Limiter limiter;
  static Series series;
  static TunerBlock tuner;
  configure(gate, eq, limiter);
  configure(series.gate, series.eq, series.limiter);
  const std::vector<float> x = input();
  char date[16];
  const std::time_t now = std::time(nullptr);
  std::strftime(date, sizeof date, "%Y-%m-%d", std::localtime(&now));
  std::printf("<!-- SPDX-License-Identifier: MIT -->\n# squatch-dsp CPU benchmark\n\n");
  std::printf("%s, %s, `-O3 -fno-exceptions -fno-rtti`, %s. `make bench` writes this file.\n\n",
              argc > 1 ? argv[1] : "unknown CPU", argc > 2 ? argv[2] : "unknown compiler", date);
  std::printf("Nanoseconds per sample at 48 kHz, by block size (best of 7 runs of 10 s of audio, one core).\n");
  std::printf("At 48 kHz one core has 20833 ns per sample; the last column is that share at 128.\n\n");
  std::printf("| block | 16 | 32 | 64 | 128 | %% of a core |\n|---|---:|---:|---:|---:|---:|\n");
  row("limiter (limiting)", limiter, x);
  row("gate (open)", gate, x);
  row("eq (3 bands)", eq, x);
  row("gate + eq + limiter", series, x);
  row("tuner (tracking 110 Hz)", tuner, x);
  return 0;
}
