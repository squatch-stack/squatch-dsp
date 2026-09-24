// SPDX-License-Identifier: MIT
// A bump allocator over memory the host owns. Blocks take everything they need in prepare();
// nothing is ever freed one piece at a time. Running out is recorded (failed()) at prepare
// time, so a host can refuse to start rather than discover it on the audio thread.
//
// An arena over no memory is a measuring arena: prepare blocks against it and read required().
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace squatch::dsp {

class Arena {
 public:
  static constexpr std::size_t kAlign = 16;  // enough for NEON/SSE loads of four floats

  Arena() = default;
  Arena(void* memory, std::size_t bytes)
      : base_(static_cast<unsigned char*>(memory)), capacity_(memory ? bytes : 0) {}

  /// `count` zeroed objects, or nullptr (and failed() from then on) when they do not fit.
  template <class T>
  T* alloc(std::size_t count) {
    static_assert(std::is_trivially_copyable<T>::value && std::is_trivially_destructible<T>::value,
                  "arena memory is zero-filled and never destroyed");
    if (count > SIZE_MAX / sizeof(T) / 2) return fail(SIZE_MAX / 2);
    const std::size_t align = alignof(T) > kAlign ? alignof(T) : kAlign;
    return static_cast<T*>(take(count * sizeof(T), align));
  }

  bool failed() const { return failed_; }
  std::size_t used() const { return used_; }
  std::size_t capacity() const { return capacity_; }
  /// Bytes every request so far would have needed (an upper bound once anything failed).
  std::size_t required() const { return required_; }

  /// Forgets every allocation; the blocks that held them must be prepared again.
  void rewind() {
    used_ = required_ = 0;
    failed_ = false;
  }

 private:
  void* take(std::size_t bytes, std::size_t align) {
    const std::uintptr_t at = reinterpret_cast<std::uintptr_t>(base_) + used_;
    const std::size_t pad = (align - at % align) % align;
    if (failed_ || pad + bytes > capacity_ - used_) return fail(bytes + align);
    required_ = used_ + pad + bytes;
    unsigned char* p = base_ + used_ + pad;
    used_ += pad + bytes;
    std::memset(p, 0, bytes);
    return p;
  }

  std::nullptr_t fail(std::size_t bytes) {
    if (!failed_) required_ = used_;
    required_ += bytes;
    failed_ = true;
    return nullptr;
  }

  unsigned char* base_ = nullptr;
  std::size_t capacity_ = 0;
  std::size_t used_ = 0;
  std::size_t required_ = 0;
  bool failed_ = false;
};

}  // namespace squatch::dsp
