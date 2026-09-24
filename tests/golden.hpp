// SPDX-License-Identifier: MIT
// Golden renders: a block's output on a fixed input, stored as a fingerprint (the RMS of each of
// 64 equal segments) in tests/golden/<name>.inc. A tolerance, not a hash, because FMA contraction
// and libm differ between the Mac, the Pi and the M7. `make golden` rewrites the files.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "check.hpp"

namespace golden {

constexpr int kSegments = 64;

inline std::vector<double> fingerprint(const std::vector<float>& y) {
  std::vector<double> f(kSegments, 0.0);
  const std::size_t len = y.size() / kSegments;
  for (int s = 0; s < kSegments; ++s) {
    double sum = 0.0;
    for (std::size_t i = 0; i < len; ++i) sum += double(y[s * len + i]) * y[s * len + i];
    f[s] = std::sqrt(sum / double(len));
  }
  return f;
}

inline void write(const char* name, const std::vector<double>& f) {
  char path[512];
  std::snprintf(path, sizeof path, "%s/%s.inc", std::getenv("SQUATCH_WRITE_GOLDEN"), name);
  FILE* out = std::fopen(path, "w");
  if (!out) return;
  std::fprintf(out, "// SPDX-License-Identifier: MIT\n// Written by `make golden`.\n{\n");
  for (double v : f) std::fprintf(out, "    %.17g,\n", v);
  std::fprintf(out, "}\n");
  std::fclose(out);
  std::printf("wrote %s\n", path);
}

/// Compares y's fingerprint with the stored one to a relative tolerance (or writes it).
inline void check(const char* name, const std::vector<float>& y, const double* stored, double tol) {
  const std::vector<double> f = fingerprint(y);
  if (std::getenv("SQUATCH_WRITE_GOLDEN")) return write(name, f);
  CHECK(stored != nullptr, "%s: no golden render; run `make golden`", name);
  if (!stored) return;
  double worst = 0.0;
  for (int s = 0; s < kSegments; ++s) worst = std::fmax(worst, std::fabs(f[s] - stored[s]) / (stored[s] + 1e-9));
  CHECK(worst <= tol, "%s: fingerprint off by %.3g (tolerance %.3g)", name, worst, tol);
}

}  // namespace golden

