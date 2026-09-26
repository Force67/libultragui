#ifndef ULTRAGUI_CORE_CONFIG_H_
#define ULTRAGUI_CORE_CONFIG_H_

/// Container type customization point.
///
/// Three configurations provide the types every ultragui header and source
/// uses:
///
///   - default: the C++ standard library.
///   - ULTRAGUI_USE_BASE: equilibrium base (base::Vector, base::String, ...),
///     with no standard library and no exceptions. The CMake option of the same
///     name defines it publicly and links equilibrium::base.
///   - ULTRAGUI_CUSTOM_CONFIG: your own header, included in place of both:
///
///       -DULTRAGUI_CUSTOM_CONFIG="my_engine/ugui_config.h"
///
/// The library is written against the API the first two share, so a custom
/// header must provide, in namespace ugui:
///
///   template <typename T> using Vector = ...;
///       std::vector's copy and move, push_back, emplace_back, pop_back,
///       front, back, operator[], data, size, empty, clear, resize, reserve,
///       shrink_to_fit, assign(n, value), assign(first, last), begin/end
///       (random access), rbegin/rend, insert(pos, value) and
///       erase(first, last), with the return values unused.
///   using String = ...;
///       std::string's c_str, data, size, empty, operator[], back, +=,
///       append(ptr, len), append(str), push_back, pop_back, resize,
///       erase(pos, n), substr, find, rfind(char), find_first_not_of(char),
///       ==, <, and + with const char*.
///   template <typename K, typename V, typename H = <default hash>>
///   class HashMap;
///       key_type, mapped_type; V& operator[](key); V* find(key) (nullptr if
///       absent); Pair<V*, bool> emplace(key, args...) (constructs only if
///       absent); bool contains(key); bool erase(key); clear, size, empty;
///       iteration whose element destructures into [key, value]. Iteration
///       order is unspecified and the library never lets it reach output.
///       Values may move when the table grows.
///   template <typename Sig> using Function = ...;   // std::function subset
///   template <typename T> using Optional = ...;     // std::optional subset;
///       a moved-from Optional is treated as unspecified (base empties it)
///   template <typename T> class UniquePtr;
///       operator->, operator*, bool, Reset(T* = nullptr), move from a
///       UniquePtr<Derived>, Get_UseOnlyIfYouKnowWhatYouareDoing().
///   template <typename T, typename... A> UniquePtr<T> MakeUnique(A&&...);
///   template <typename A, typename B> Pair;  // .first/.second, brace init
///   move(x), forward<T>(x)                   // std::move/std::forward

#if defined(ULTRAGUI_CUSTOM_CONFIG)
#include ULTRAGUI_CUSTOM_CONFIG

#elif defined(ULTRAGUI_USE_BASE)

#include <base/containers/pair.h>
#include <base/containers/unordered_map.h>
#include <base/containers/vector.h>
#include <base/functional/function.h>
#include <base/memory/move.h>
#include <base/memory/unique_pointer.h>
#include <base/optional.h>
#include <base/strings/xstring.h>

namespace ugui {

template <typename T>
using Vector = base::Vector<T>;

using String = base::String;

template <typename K, typename V, typename H = base::Hash<K> >
using HashMap = base::UnorderedMap<K, V, H>;

template <typename Sig>
using Function = base::Function<Sig>;

template <typename T>
using Optional = base::Optional<T>;

template <typename T>
using UniquePtr = base::UniquePointer<T>;

template <typename T, typename... Args>
UniquePtr<T> MakeUnique(Args&&... args) {
  return base::MakeUnique<T>(base::forward<Args>(args)...);
}

template <typename A, typename B>
using Pair = base::Pair<A, B>;

using base::forward;
using base::move;

}  // namespace ugui

#else

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ugui {

template <typename T>
using Vector = std::vector<T>;

using String = std::string;

template <typename A, typename B>
using Pair = std::pair<A, B>;

/// std::unordered_map in base::UnorderedMap's shape: find returns the value
/// pointer, erase reports whether the key was present, and iteration yields
/// {key, value}.
template <typename K, typename V, typename H = std::hash<K> >
class HashMap {
  using Map = std::unordered_map<K, V, H>;

 public:
  using key_type = K;
  using mapped_type = V;

  struct KeyValueRef {
    const K& key;
    V& value;
  };
  struct ConstKeyValueRef {
    const K& key;
    const V& value;
  };

  class Iterator {
   public:
    explicit Iterator(typename Map::iterator it) : it_(it) {}
    bool operator==(const Iterator& o) const { return it_ == o.it_; }
    bool operator!=(const Iterator& o) const { return it_ != o.it_; }
    Iterator& operator++() {
      ++it_;
      return *this;
    }
    KeyValueRef operator*() const { return {it_->first, it_->second}; }

   private:
    typename Map::iterator it_;
  };

  class ConstIterator {
   public:
    explicit ConstIterator(typename Map::const_iterator it) : it_(it) {}
    bool operator==(const ConstIterator& o) const { return it_ == o.it_; }
    bool operator!=(const ConstIterator& o) const { return it_ != o.it_; }
    ConstIterator& operator++() {
      ++it_;
      return *this;
    }
    ConstKeyValueRef operator*() const { return {it_->first, it_->second}; }

   private:
    typename Map::const_iterator it_;
  };

  V& operator[](const K& key) { return map_[key]; }
  V* find(const K& key) {
    auto it = map_.find(key);
    return it == map_.end() ? nullptr : &it->second;
  }
  const V* find(const K& key) const {
    auto it = map_.find(key);
    return it == map_.end() ? nullptr : &it->second;
  }
  V& at(const K& key) { return map_.at(key); }
  const V& at(const K& key) const { return map_.at(key); }
  /// Constructs the value only if `key` is absent; returns the stored value
  /// and whether it was inserted.
  template <typename... Args>
  Pair<V*, bool> emplace(const K& key, Args&&... args) {
    auto [it, inserted] = map_.try_emplace(key, std::forward<Args>(args)...);
    return {&it->second, inserted};
  }
  bool contains(const K& key) const { return map_.count(key) != 0; }
  bool erase(const K& key) { return map_.erase(key) != 0; }
  void clear() { map_.clear(); }
  typename Map::size_type size() const { return map_.size(); }
  bool empty() const { return map_.empty(); }
  void reserve(typename Map::size_type count) { map_.reserve(count); }

  Iterator begin() { return Iterator(map_.begin()); }
  Iterator end() { return Iterator(map_.end()); }
  ConstIterator begin() const { return ConstIterator(map_.begin()); }
  ConstIterator end() const { return ConstIterator(map_.end()); }

 private:
  Map map_;
};

template <typename Sig>
using Function = std::function<Sig>;

template <typename T>
using Optional = std::optional<T>;

/// std::unique_ptr in base::UniquePointer's shape: no get()/reset()/release().
template <typename T>
class UniquePtr : public std::unique_ptr<T> {
 public:
  using std::unique_ptr<T>::unique_ptr;
  T* Get_UseOnlyIfYouKnowWhatYouareDoing() const { return this->get(); }
  void Reset(T* p = nullptr) { this->reset(p); }

 private:
  using std::unique_ptr<T>::get;
  using std::unique_ptr<T>::release;
  using std::unique_ptr<T>::reset;
};

template <typename T, typename... Args>
UniquePtr<T> MakeUnique(Args&&... args) {
  return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

using std::forward;
using std::move;

}  // namespace ugui

#endif  // ULTRAGUI_CUSTOM_CONFIG / ULTRAGUI_USE_BASE

#endif  // ULTRAGUI_CORE_CONFIG_H_
