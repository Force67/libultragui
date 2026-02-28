#ifndef ULTRAGUI_CORE_ARENA_H_
#define ULTRAGUI_CORE_ARENA_H_

#include <ultragui/core/types.h>

#include <cstdlib>
#include <new>
#include <utility>

namespace ugui {

/// Simple linear/bump allocator. Allocations are fast (pointer bump).
/// Memory is freed all at once via reset() or destructor.
class Arena {
public:
    explicit Arena(usize capacity = 1024 * 1024);
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&& other) noexcept;
    Arena& operator=(Arena&& other) noexcept;

    /// Allocate `size` bytes with given alignment
    void* Alloc(usize size, usize alignment = alignof(std::max_align_t));

    /// Allocate and construct a T
    template <typename T, typename... Args> T* create(Args&&... args) {
        void* mem = Alloc(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    /// Allocate an array of T (default-constructed)
    template <typename T> T* AllocArray(usize count) {
        void* mem = Alloc(sizeof(T) * count, alignof(T));
        T* arr = static_cast<T*>(mem);
        for (usize i = 0; i < count; ++i) {
            new (&arr[i]) T();
        }
        return arr;
    }

    /// Reset allocator: all prior allocations become invalid
    void reset();

    usize used() const { return offset_; }
    usize capacity() const { return capacity_; }
    usize remaining() const { return capacity_ - offset_; }

private:
    u8* buffer_ = nullptr;
    usize capacity_ = 0;
    usize offset_ = 0;
};

/// RAII scope guard that resets the arena on destruction.
/// Useful for per-frame temporary allocations.
class ArenaScope {
public:
    explicit ArenaScope(Arena& arena) : arena_(arena), saved_offset_(arena.used()) {}
    ~ArenaScope() { arena_.reset(); }

    ArenaScope(const ArenaScope&) = delete;
    ArenaScope& operator=(const ArenaScope&) = delete;

private:
    Arena& arena_;
    usize saved_offset_;
};

} // namespace ugui

#endif  // ULTRAGUI_CORE_ARENA_H_
