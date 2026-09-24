// SPDX-License-Identifier: MIT
#include <squatch/dsp/chain.hpp>
#include <squatch/dsp/denormal.hpp>
#include <squatch/dsp/span.hpp>

namespace squatch::dsp {

bool Chain::add(Block& b) {
  if (count_ == kMaxBlocks) return false;
  blocks_[count_++] = &b;
  return true;
}

void Chain::prepare(const Setup& s, Arena& mem) {
  maxBlock_ = s.maxBlock;
  for (int i = 0; i < count_; ++i) blocks_[i]->prepare(s, mem);
}

void Chain::process(const float* in, float* out, int n) {
  DenormalGuard ftz;
  const int chunk = maxBlock_ > 0 ? maxBlock_ : n;
  for (int done = 0; done < n; done += chunk) {
    const int len = n - done < chunk ? n - done : chunk;
    processChunk(in + done, out + done, len);
  }
}

void Chain::processChunk(const float* in, float* out, int n) {
  if (count_ == 0) return copy(in, out, n);
  blocks_[0]->process(in, out, n);
  for (int i = 1; i < count_; ++i) blocks_[i]->process(out, out, n);
}

void Chain::reset() {
  for (int i = 0; i < count_; ++i) blocks_[i]->reset();
}

int Chain::latency() const {
  int total = 0;
  for (int i = 0; i < count_; ++i) total += blocks_[i]->latency();
  return total;
}

bool Chain::set(int index, ParamId id, float value) {
  if (index < 0 || index >= count_) return false;
  blocks_[index]->set(id, value);
  return true;
}

}  // namespace squatch::dsp
