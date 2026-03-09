#include <ultragui/core/arena.h>

#include <cassert>
#include <cstdlib>

namespace ugui {

Arena::Arena(usize capacity) : capacity_(capacity), offset_(0) {
  buffer_ = static_cast<u8*>(std::malloc(capacity));
  assert(buffer_ && "Arena: allocation failed");
}

Arena::~Arena() { std::free(buffer_); }

Arena::Arena(Arena&& other) noexcept
    : buffer_(other.buffer_),
      capacity_(other.capacity_),
      offset_(other.offset_) {
  other.buffer_ = nullptr;
  other.capacity_ = 0;
  other.offset_ = 0;
}

Arena& Arena::operator=(Arena&& other) noexcept {
  if (this != &other) {
    std::free(buffer_);
    buffer_ = other.buffer_;
    capacity_ = other.capacity_;
    offset_ = other.offset_;
    other.buffer_ = nullptr;
    other.capacity_ = 0;
    other.offset_ = 0;
  }
  return *this;
}

void* Arena::Alloc(usize size, usize alignment) {
  usize aligned = (offset_ + alignment - 1) & ~(alignment - 1);
  assert(aligned + size <= capacity_ && "Arena: out of memory");
  void* ptr = buffer_ + aligned;
  offset_ = aligned + size;
  return ptr;
}

void Arena::reset() { offset_ = 0; }

}  // namespace ugui
