<!-- SPDX-License-Identifier: MIT -->
# squatch-dsp CPU benchmark

Apple M1 Max, Apple clang version 21.0.0 (clang-2100.3.34.2), `-O3 -fno-exceptions -fno-rtti`, 2026-09-24. `make bench` writes this file.

Nanoseconds per sample at 48 kHz, by block size (best of 7 runs of 10 s of audio, one core).
At 48 kHz one core has 20833 ns per sample; the last column is that share at 128.

| block | 16 | 32 | 64 | 128 | % of a core |
|---|---:|---:|---:|---:|---:|
| limiter (limiting) | 50.96 | 50.83 | 50.30 | 50.36 | 0.24% |
| gate (open) | 4.69 | 4.77 | 4.66 | 4.66 | 0.02% |
| eq (3 bands) | 6.21 | 8.09 | 10.49 | 11.65 | 0.06% |
| gate + eq + limiter | 68.44 | 65.04 | 66.36 | 68.01 | 0.32% |
| tuner (tracking 110 Hz) | 131.33 | 131.55 | 130.53 | 130.77 | 0.63% |
