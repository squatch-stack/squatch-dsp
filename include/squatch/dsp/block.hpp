// SPDX-License-Identifier: MIT
// The one interface every block implements (ROADMAP.md). Mono in, mono out.
//
// The contract, which the tests enforce for every block:
// - prepare() takes all memory from the arena and may be slow; it is never called while audio runs.
//   If the arena runs out, the block stays silent (writes zeros) until prepared with enough.
// - process() is real-time safe: no allocation, locks, I/O or exceptions. 1 <= n <= maxBlock,
//   and `in` may equal `out` (in-place).
// - set() is real-time safe too and is called on the audio thread; other threads reach it through
//   a ParamQueue. Values are clamped to the descriptor's range and smoothed inside the block.
// - reset() clears state (delay lines, envelopes) but keeps memory and parameters.
#pragma once
#include <cstdint>

#include "arena.hpp"

namespace squatch::dsp {

using ParamId = std::uint16_t;

struct Setup {
  double sampleRate = 48000.0;
  int maxBlock = 128;  // the most samples one process() call will ever be given
};

struct Block {
  virtual void prepare(const Setup& s, Arena& mem) = 0;
  virtual void process(const float* in, float* out, int n) = 0;
  virtual void reset() = 0;
  virtual void set(ParamId id, float value) = 0;
  virtual int latency() const { return 0; }  // samples, for aligning parallel branches

 protected:
  // Blocks live in static or host-owned storage and are never deleted through this interface,
  // so no virtual destructor (which would pull operator delete into bare-metal builds).
  ~Block() = default;
};

}  // namespace squatch::dsp
