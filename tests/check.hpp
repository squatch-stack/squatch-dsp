// SPDX-License-Identifier: MIT
// The test runner, without a framework: TEST(name) registers a function, CHECK counts checks
// and failures, and main() (runner.cpp) runs every test or those whose name contains argv[1].
#pragma once
#include <cstdio>

namespace test {

struct Counts {
  int tests = 0, checks = 0, failures = 0;
};

Counts& counts();
bool add(const char* name, void (*fn)());

}  // namespace test

#define TEST(name)                                         \
  static void name();                                      \
  [[maybe_unused]] static const bool name##Registered =    \
      test::add(#name, name);                              \
  static void name()

#define CHECK(cond, ...)                                          \
  do {                                                            \
    ++test::counts().checks;                                      \
    if (!(cond)) {                                                \
      ++test::counts().failures;                                  \
      std::printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); \
      std::printf(__VA_ARGS__);                                   \
      std::printf("\n");                                          \
    }                                                             \
  } while (0)
