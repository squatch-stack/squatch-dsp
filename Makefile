# SPDX-License-Identifier: MIT
# squatch-dsp: the core library, its tests and benchmarks. System clang++, no fetched dependencies.
# clang++ where it exists (this Mac), g++ otherwise (the Pi). CXX=... on the command line wins.
ifeq ($(origin CXX),default)
CXX := $(if $(shell command -v clang++ 2>/dev/null),clang++,g++)
endif
ifeq ($(origin CC),default)
CC := $(if $(shell command -v clang 2>/dev/null),clang,gcc)
endif
BUILD := build
UNAME := $(shell uname -s)

WARN := -Wall -Wextra -Wpedantic -Wshadow -Werror
CORE := -std=c++17 -fno-exceptions -fno-rtti $(WARN) -Iinclude -Iblocks/tuner/include
RELEASE := $(CORE) -O3
SANITIZE := $(CORE) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all
TSAN := $(CORE) -O1 -g -fsanitize=thread

HEADERS := $(shell find include -name '*.hpp')
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=$(BUILD)/obj/%.o)
TEST_SRC := $(filter-out tests/test_queue_threads.cpp,$(wildcard tests/*.cpp))
TEST_DEPS := $(wildcard tests/*.hpp) $(wildcard tests/golden/*.inc)

# Locks taken inside process() are counted by an interposing dylib (dyld honours __interpose in
# a linked library). Elsewhere the lock check is reported as unmeasured, never as passed.
ifeq ($(UNAME),Darwin)
LOCKS := $(BUILD)/liblockcount.dylib
LOCK_LINK := -L$(BUILD) -llockcount -Wl,-rpath,@loader_path
BENCH_OUT ?= results/bench-mac.md
else
TEST_DEFS := -DSQUATCH_NO_LOCK_COUNT
BENCH_OUT ?= results/bench-$(shell uname -m).md
endif

.PHONY: all lib test lint headers bench golden ci live wasm wasm-test wasm-shots clean
all: lib $(BUILD)/tests $(BUILD)/bench

# The core as the targets build it: optimised, no exceptions, no RTTI.
lib: $(BUILD)/libsquatch-dsp.a

$(BUILD)/obj/%.o: src/%.cpp $(HEADERS) | $(BUILD)/obj
	$(CXX) $(RELEASE) -c -o $@ $<

$(BUILD)/libsquatch-dsp.a: $(OBJ)
	ar rcs $@ $^

$(BUILD)/liblockcount.dylib: tests/lockcount.c | $(BUILD)
	$(CC) -O1 -Wall -Wextra -Werror -dynamiclib -o $@ $<

# Tests compile the sources again with ASan and UBSan.
$(BUILD)/tests: $(TEST_SRC) $(SRC) $(HEADERS) $(TEST_DEPS) $(LOCKS) | $(BUILD)
	$(CXX) $(SANITIZE) $(TEST_DEFS) -o $@ $(TEST_SRC) $(SRC) $(LOCK_LINK)

# The parameter queue is tested across threads, so it gets ThreadSanitizer instead.
$(BUILD)/test_queue_threads: tests/test_queue_threads.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(TSAN) -o $@ $< -lpthread

# Hosts (hosts/): not core, so exceptions are allowed, but the same warnings. The host layer's
# handoff is tested for data races with TSan, which refuses the Pi 5's 47-bit address space:
# there, `make test HOST_SAN=`.
HOST := -std=c++17 -O3 $(WARN) -Iinclude -Iblocks/tuner/include
HOST_SAN ?= -fsanitize=thread
HOST_HEADERS := $(wildcard hosts/common/*.hpp) $(wildcard hosts/jack/*.hpp) $(HEADERS)
TUNER_HEADERS := $(wildcard blocks/tuner/include/squatch/tuner/*.hpp)

$(BUILD)/test_hosts: hosts/tests/test_hosts.cpp $(HOST_HEADERS) $(TUNER_HEADERS) | $(BUILD)
	$(CXX) $(HOST) -O1 -g $(HOST_SAN) -o $@ $< -lpthread

# The live JACK tuner and its test-tone generator. Needs the JACK headers (libjack-jackd2-dev):
# built on the Pi bench, not part of `all`. hosts/pi/ expects them in build/.
live: $(BUILD)/squatch-tuner-live $(BUILD)/squatch-tone

$(BUILD)/squatch-tuner-live: hosts/jack/tuner_live.cpp $(HOST_HEADERS) $(TUNER_HEADERS) | $(BUILD)
	$(CXX) $(HOST) -o $@ $< -ljack -lpthread

$(BUILD)/squatch-tone: hosts/jack/tone.cpp $(HOST_HEADERS) | $(BUILD)
	$(CXX) $(HOST) -o $@ $< -ljack -lpthread

# The browser demo (hosts/wasm): the tuner as WebAssembly in an AudioWorklet, a static folder
# for any HTTPS host. Needs Emscripten on PATH (source <emsdk>/emsdk_env.sh); not part of `all`.
# wasm-test scores the module in Node, then the page in headless Chrome against test tones.
DEMO := $(BUILD)/web-demo
wasm:
	python3 hosts/wasm/build.py $(DEMO)

wasm-test: wasm
	node hosts/wasm/test/core.test.mjs $(DEMO)/tuner.wasm
	node hosts/wasm/test/browser.test.mjs --dir $(DEMO)

# Rewrites the demo screenshots in blocks/tuner/docs/screens/ (desktop and phone, light and dark).
wasm-shots: wasm
	node hosts/wasm/test/browser.test.mjs --dir $(DEMO) --only a2,a2-plus7 --shots blocks/tuner/docs/screens

$(BUILD)/bench: bench/bench.cpp $(BUILD)/libsquatch-dsp.a $(HEADERS) | $(BUILD)
	$(CXX) $(RELEASE) -o $@ $< $(BUILD)/libsquatch-dsp.a

$(BUILD) $(BUILD)/obj:
	mkdir -p $@

test: $(BUILD)/tests $(BUILD)/test_queue_threads $(BUILD)/test_hosts
	./$(BUILD)/tests
	./$(BUILD)/test_queue_threads
	./$(BUILD)/test_hosts
	$(MAKE) -C blocks/tuner test CXX=$(CXX)

# Rewrites tests/golden/*.inc from the current code. Only after a deliberate change of sound.
golden: $(BUILD)/tests
	SQUATCH_WRITE_GOLDEN=tests/golden ./$(BUILD)/tests Golden
	$(MAKE) test

bench: $(BUILD)/bench
	mkdir -p results
	./$(BUILD)/bench "$$(sysctl -n machdep.cpu.brand_string 2>/dev/null || uname -m)" \
	  "$$($(CXX) --version | head -1)" > $(BENCH_OUT)
	cat $(BENCH_OUT)

# Each public header compiles on its own, with the core's flags.
headers:
	@for h in $(HEADERS); do \
	  printf '#include <%s>\n' "$${h#include/}" | $(CXX) $(CORE) -x c++ -fsyntax-only - || exit 1; \
	done; echo "headers: $(words $(HEADERS)) compile standalone"

# Complexity limits of the Squatch project kit: CCN 10, 60 lines, 5 parameters per function.
lint:
	@command -v lizard >/dev/null || { echo "lizard not found: this repo installs nothing"; exit 2; }
	lizard -C 10 -L 60 -a 5 -w include src tests bench hosts blocks/tuner/include blocks/tuner/bench blocks/tuner/tests

ci: lint headers lib test

clean:
	rm -rf $(BUILD)
