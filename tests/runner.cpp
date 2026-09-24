// SPDX-License-Identifier: MIT
#include <cstring>

#include "check.hpp"

namespace test {
namespace {

struct Case {
  const char* name;
  void (*fn)();
};

Case gCases[256];
int gCount = 0;

}  // namespace

Counts& counts() {
  static Counts c;
  return c;
}

bool add(const char* name, void (*fn)()) {
  if (gCount == 256) return false;
  gCases[gCount++] = {name, fn};
  return true;
}

}  // namespace test

int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);  // keep our lines in order with sanitizer reports
  const char* filter = argc > 1 ? argv[1] : "";
  for (int i = 0; i < test::gCount; ++i) {
    if (!std::strstr(test::gCases[i].name, filter)) continue;
    const int before = test::counts().failures;
    ++test::counts().tests;
    test::gCases[i].fn();
    std::printf("%s %s\n", test::counts().failures == before ? "ok  " : "FAIL", test::gCases[i].name);
  }
  const test::Counts& c = test::counts();
  std::printf("%d tests, %d checks, %d failure(s)\n", c.tests, c.checks, c.failures);
  return c.failures || c.tests == 0 ? 1 : 0;
}
