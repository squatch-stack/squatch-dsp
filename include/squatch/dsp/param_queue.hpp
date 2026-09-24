// SPDX-License-Identifier: MIT
// Lock-free single-producer/single-consumer queue: a UI or control thread pushes parameter
// changes, the audio thread pops them before each process() call. Fixed capacity, no heap.
// A full queue refuses the push; the producer keeps its latest value and tries again later.
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "block.hpp"

namespace squatch::dsp {

template <class T, std::size_t Capacity>
class SpscQueue {
  static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "capacity is a power of two");
  static_assert(std::atomic<std::size_t>::is_always_lock_free, "the indices must be lock-free");

 public:
  /// Producer thread only.
  bool push(const T& v) {
    const std::size_t w = write_.load(std::memory_order_relaxed);
    if (w - read_.load(std::memory_order_acquire) == Capacity) return false;
    slots_[w & (Capacity - 1)] = v;
    write_.store(w + 1, std::memory_order_release);
    return true;
  }

  /// Consumer thread only.
  bool pop(T& v) {
    const std::size_t r = read_.load(std::memory_order_relaxed);
    if (r == write_.load(std::memory_order_acquire)) return false;
    v = slots_[r & (Capacity - 1)];
    read_.store(r + 1, std::memory_order_release);
    return true;
  }

 private:
  alignas(64) std::atomic<std::size_t> write_{0};  // separate cache lines: no false sharing
  alignas(64) std::atomic<std::size_t> read_{0};
  T slots_[Capacity] = {};
};

struct ParamChange {
  std::uint16_t target;  // which block, e.g. its index in a Chain
  ParamId id;
  float value;
};

template <std::size_t Capacity>
using ParamQueue = SpscQueue<ParamChange, Capacity>;

}  // namespace squatch::dsp
