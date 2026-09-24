// SPDX-License-Identifier: MIT
// Denormal protection. Recursive filters decaying towards silence produce denormals, which cost
// 10-100x per operation on many CPUs. DenormalGuard sets flush-to-zero (and denormals-are-zero
// on x86) for the current thread while it lives; wrap each audio callback in one. Targets without
// such a mode (the ESP32-S3) rely on flushDenormal() on filter state instead.
#pragma once
#include <cmath>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64) || (defined(__i386__) && defined(__SSE__))
#include <xmmintrin.h>
#define SQUATCH_DSP_FTZ_X86 1
#elif defined(__aarch64__)
#define SQUATCH_DSP_FTZ_A64 1
#elif defined(__arm__) && defined(__ARM_FP)
#define SQUATCH_DSP_FTZ_A32 1
#endif

namespace squatch::dsp {

class DenormalGuard {
 public:
  DenormalGuard() : saved_(read()) { write(saved_ | kFlushBits); }
  ~DenormalGuard() { write(saved_); }
  DenormalGuard(const DenormalGuard&) = delete;
  DenormalGuard& operator=(const DenormalGuard&) = delete;

  static constexpr bool supported() { return kFlushBits != 0; }

 private:
#if defined(SQUATCH_DSP_FTZ_X86)
  static constexpr std::uint64_t kFlushBits = 0x8040;  // MXCSR FTZ | DAZ
  static std::uint64_t read() { return _mm_getcsr(); }
  static void write(std::uint64_t v) { _mm_setcsr(static_cast<unsigned>(v)); }
#elif defined(SQUATCH_DSP_FTZ_A64)
  static constexpr std::uint64_t kFlushBits = 1ull << 24;  // FPCR.FZ
  static std::uint64_t read() {
    std::uint64_t v;
    __asm__ volatile("mrs %0, fpcr" : "=r"(v));
    return v;
  }
  static void write(std::uint64_t v) { __asm__ volatile("msr fpcr, %0" : : "r"(v)); }
#elif defined(SQUATCH_DSP_FTZ_A32)
  static constexpr std::uint64_t kFlushBits = 1u << 24;  // FPSCR.FZ (Cortex-M7, Cortex-A)
  static std::uint64_t read() {
    std::uint32_t v;
    __asm__ volatile("vmrs %0, fpscr" : "=r"(v));
    return v;
  }
  static void write(std::uint64_t v) { __asm__ volatile("vmsr fpscr, %0" : : "r"(static_cast<std::uint32_t>(v))); }
#else
  static constexpr std::uint64_t kFlushBits = 0;
  static std::uint64_t read() { return 0; }
  static void write(std::uint64_t) {}
#endif
  std::uint64_t saved_;
};

/// Zero for anything below about -600 dBFS: far under any audible level, far above denormals.
inline float flushDenormal(float x) { return std::fabs(x) < 1e-30f ? 0.0f : x; }

}  // namespace squatch::dsp
