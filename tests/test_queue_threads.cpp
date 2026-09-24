// SPDX-License-Identifier: MIT
// The parameter queue across two threads, under ThreadSanitizer: a control thread pushes a
// million changes while an "audio" thread pops them; none is lost, duplicated or reordered.
#include <cstdio>
#include <squatch/dsp/param_queue.hpp>
#include <thread>

using namespace squatch::dsp;

namespace {

ParamQueue<64> gQueue;
constexpr int kCount = 1000000;

ParamChange change(int i) {
  return {static_cast<std::uint16_t>(i & 7), static_cast<ParamId>(i & 3), static_cast<float>(i)};
}

void produce() {
  for (int i = 0; i < kCount;)
    if (gQueue.push(change(i))) ++i;
}

}  // namespace

int main() {
  std::thread producer(produce);
  int expect = 0, bad = 0;
  while (expect < kCount) {
    ParamChange c;
    if (!gQueue.pop(c)) continue;
    const ParamChange want = change(expect++);
    bad += c.value != want.value || c.target != want.target || c.id != want.id;
  }
  producer.join();
  if (bad) std::printf("queue: %d of %d changes wrong\n", bad, kCount);
  else std::printf("queue: %d changes across threads, in order (TSan clean)\n", kCount);
  return bad ? 1 : 0;
}
