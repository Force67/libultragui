#ifndef ULTRAGUI_CORE_ALGORITHM_H_
#define ULTRAGUI_CORE_ALGORITHM_H_

#include <ugui/core/types.h>

namespace ugui {

// Min/Max/ClampMinMax compare exactly as libstdc++'s std::min, std::max and
// std::clamp do, so both container configurations produce the same numbers.
// base::Min/Max/Clamp compare the other way round and disagree on NaN and on
// which of -0 and +0 wins; do not swap them in.

/// std::min: the second argument only when it is strictly smaller.
template <typename T>
constexpr T Min(T a, T b) {
  return b < a ? b : a;
}

/// std::max: the second argument only when the first is strictly smaller.
template <typename T>
constexpr T Max(T a, T b) {
  return a < b ? b : a;
}

/// std::clamp as libstdc++ spells it, min(max(v, lo), hi): NaN passes through
/// and hi wins when lo > hi. Not the same as Clamp() in core/math.h, which
/// returns lo for v < lo even when lo > hi.
template <typename T>
constexpr T ClampMinMax(T v, T lo, T hi) {
  return Min(Max(v, lo), hi);
}

namespace detail {

template <typename T>
void IterSwap(T* a, T* b) {
  T tmp = ugui::move(*a);
  *a = ugui::move(*b);
  *b = ugui::move(tmp);
}

template <typename T, typename Less>
void MoveMedianToFirst(T* result, T* a, T* b, T* c, Less& less) {
  if (less(*a, *b)) {
    if (less(*b, *c))
      IterSwap(result, b);
    else if (less(*a, *c))
      IterSwap(result, c);
    else
      IterSwap(result, a);
  } else if (less(*a, *c)) {
    IterSwap(result, a);
  } else if (less(*b, *c)) {
    IterSwap(result, c);
  } else {
    IterSwap(result, b);
  }
}

template <typename T, typename Less>
T* UnguardedPartition(T* first, T* last, T* pivot, Less& less) {
  while (true) {
    while (less(*first, *pivot)) ++first;
    --last;
    while (less(*pivot, *last)) --last;
    if (!(first < last)) return first;
    IterSwap(first, last);
    ++first;
  }
}

template <typename T, typename Less>
void PushHeap(T* first, ptrdiff_t hole, ptrdiff_t top, T value, Less& less) {
  ptrdiff_t parent = (hole - 1) / 2;
  while (hole > top && less(first[parent], value)) {
    first[hole] = ugui::move(first[parent]);
    hole = parent;
    parent = (hole - 1) / 2;
  }
  first[hole] = ugui::move(value);
}

template <typename T, typename Less>
void AdjustHeap(T* first, ptrdiff_t hole, ptrdiff_t len, T value, Less& less) {
  const ptrdiff_t top = hole;
  ptrdiff_t child = hole;
  while (child < (len - 1) / 2) {
    child = 2 * (child + 1);
    if (less(first[child], first[child - 1])) child--;
    first[hole] = ugui::move(first[child]);
    hole = child;
  }
  if ((len & 1) == 0 && child == (len - 2) / 2) {
    child = 2 * (child + 1);
    first[hole] = ugui::move(first[child - 1]);
    hole = child - 1;
  }
  PushHeap(first, hole, top, ugui::move(value), less);
}

template <typename T, typename Less>
void HeapSort(T* first, T* last, Less& less) {
  const ptrdiff_t len = last - first;
  if (len >= 2) {
    for (ptrdiff_t parent = (len - 2) / 2;; --parent) {
      T value = ugui::move(first[parent]);
      AdjustHeap(first, parent, len, ugui::move(value), less);
      if (parent == 0) break;
    }
  }
  while (last - first > 1) {
    --last;
    T value = ugui::move(*last);
    *last = ugui::move(*first);
    AdjustHeap(first, ptrdiff_t{0}, last - first, ugui::move(value), less);
  }
}

template <typename T, typename Less>
void UnguardedLinearInsert(T* last, Less& less) {
  T value = ugui::move(*last);
  T* next = last - 1;
  while (less(value, *next)) {
    *last = ugui::move(*next);
    last = next;
    --next;
  }
  *last = ugui::move(value);
}

template <typename T, typename Less>
void InsertionSort(T* first, T* last, Less& less) {
  if (first == last) return;
  for (T* i = first + 1; i != last; ++i) {
    if (less(*i, *first)) {
      T value = ugui::move(*i);
      for (T* p = i; p != first; --p) *p = ugui::move(*(p - 1));
      *first = ugui::move(value);
    } else {
      UnguardedLinearInsert(i, less);
    }
  }
}

inline constexpr ptrdiff_t kIntroSortThreshold = 16;

template <typename T, typename Less>
void IntroSortLoop(T* first, T* last, ptrdiff_t depth_limit, Less& less) {
  while (last - first > kIntroSortThreshold) {
    if (depth_limit == 0) {
      HeapSort(first, last, less);
      return;
    }
    --depth_limit;
    T* mid = first + (last - first) / 2;
    MoveMedianToFirst(first, first + 1, mid, last - 1, less);
    T* cut = UnguardedPartition(first + 1, last, first, less);
    IntroSortLoop(cut, last, depth_limit, less);
    last = cut;
  }
}

}  // namespace detail

/// Sorts [first, last) exactly as libstdc++'s std::sort does: the same
/// introsort, step for step, so equal elements end up in the same (unstable)
/// order it would leave them in. Call sites where ties can occur and their
/// order reaches output rely on that to render as the STL build always has.
template <typename T, typename Less>
void IntroSort(T* first, T* last, Less less) {
  if (first == last) return;
  ptrdiff_t lg = 0;
  for (ptrdiff_t n = last - first; n > 1; n >>= 1) ++lg;
  detail::IntroSortLoop(first, last, lg * 2, less);
  if (last - first > detail::kIntroSortThreshold) {
    T* mid = first + detail::kIntroSortThreshold;
    detail::InsertionSort(first, mid, less);
    for (T* i = mid; i != last; ++i) detail::UnguardedLinearInsert(i, less);
  } else {
    detail::InsertionSort(first, last, less);
  }
}

template <typename C, typename Less>
void IntroSort(C& container, Less less) {
  IntroSort(container.data(), container.data() + container.size(), less);
}

/// The entries of a HashMap as (key, value) pointers in ascending key order.
/// HashMap iteration order differs between the STL and base configurations,
/// so anything whose result depends on the order walks this instead. The
/// pointers are valid until the map is next modified.
template <typename Map>
Vector<Pair<const typename Map::key_type*, const typename Map::mapped_type*>>
SortedEntries(const Map& map) {
  using K = typename Map::key_type;
  using V = typename Map::mapped_type;
  Vector<Pair<const K*, const V*>> entries;
  entries.reserve(map.size());
  for (const auto& [key, value] : map) entries.push_back({&key, &value});
  // Keys are unique, so the order is fully determined.
  IntroSort(entries, [](const Pair<const K*, const V*>& a,
                        const Pair<const K*, const V*>& b) {
    return *a.first < *b.first;
  });
  return entries;
}

/// Erase every element matching `pred`, keeping the survivors in order (what
/// erase(remove_if(...)) does).
template <typename C, typename Pred>
void EraseIf(C& container, Pred pred) {
  usize kept = 0;
  const usize n = container.size();
  for (usize i = 0; i < n; ++i) {
    if (pred(container[i])) continue;
    if (kept != i) container[kept] = ugui::move(container[i]);
    ++kept;
  }
  container.erase(container.begin() + kept, container.end());
}

/// Index of the first element equal to `value`, or `container.size()`.
template <typename C, typename T>
usize IndexOf(const C& container, const T& value) {
  const usize n = container.size();
  for (usize i = 0; i < n; ++i) {
    if (container[i] == value) return i;
  }
  return n;
}

}  // namespace ugui

#endif  // ULTRAGUI_CORE_ALGORITHM_H_
