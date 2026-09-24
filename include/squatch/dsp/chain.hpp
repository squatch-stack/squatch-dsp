// SPDX-License-Identifier: MIT
// Blocks in series: the embryo of the M3 graph, kept minimal. The chain does not own its blocks.
#pragma once
#include "block.hpp"
#include "param_queue.hpp"

namespace squatch::dsp {

class Chain {
 public:
  static constexpr int kMaxBlocks = 16;

  /// Setup time only. False when the chain is full.
  bool add(Block& b);
  void prepare(const Setup& s, Arena& mem);
  /// Any n: calls longer than maxBlock are split. Flush-to-zero is on for the duration.
  void process(const float* in, float* out, int n);
  void reset();
  int latency() const;
  int size() const { return count_; }

  /// False when `index` names no block.
  bool set(int index, ParamId id, float value);

  /// Applies every queued change; call on the audio thread before process().
  template <std::size_t N>
  void apply(ParamQueue<N>& queue) {
    ParamChange c;
    while (queue.pop(c)) set(c.target, c.id, c.value);
  }

 private:
  void processChunk(const float* in, float* out, int n);

  Block* blocks_[kMaxBlocks] = {};
  int count_ = 0;
  int maxBlock_ = 0;
};

}  // namespace squatch::dsp
