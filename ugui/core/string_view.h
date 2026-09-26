#ifndef ULTRAGUI_CORE_STRING_VIEW_H_
#define ULTRAGUI_CORE_STRING_VIEW_H_

#include <ugui/core/types.h>

namespace ugui {

/// Non-owning string reference with std::string_view's semantics for the
/// members it has (substr clamps count; find returns npos on a miss).
class StringView {
 public:
  static constexpr usize npos = static_cast<usize>(-1);

  constexpr StringView() = default;
  constexpr StringView(const char* str)
      : data_(str ? str : ""), size_(str ? Length(str) : 0) {}
  constexpr StringView(const char* str, usize len) : data_(str), size_(len) {}
  StringView(const String& str) : data_(str.data()), size_(str.size()) {}

  constexpr const char* data() const { return data_; }
  constexpr usize size() const { return size_; }
  constexpr bool empty() const { return size_ == 0; }

  constexpr char operator[](usize i) const { return data_[i]; }

  /// Precondition: pos <= size() (std::string_view throws; this is a bug).
  constexpr StringView substr(usize pos, usize count = npos) const {
    usize rest = size_ - pos;
    return StringView(data_ + pos, count < rest ? count : rest);
  }

  constexpr void remove_prefix(usize n) {
    data_ += n;
    size_ -= n;
  }

  constexpr usize find(char c, usize pos = 0) const {
    for (usize i = pos; i < size_; ++i) {
      if (data_[i] == c) return i;
    }
    return npos;
  }

  constexpr usize find(StringView s, usize pos = 0) const {
    if (pos > size_ || s.size_ > size_ - pos) return npos;
    for (usize i = pos; i + s.size_ <= size_; ++i) {
      if (Equal(data_ + i, s.data_, s.size_)) return i;
    }
    return npos;
  }

  constexpr bool starts_with(StringView prefix) const {
    return size_ >= prefix.size_ && Equal(data_, prefix.data_, prefix.size_);
  }
  constexpr bool ends_with(StringView suffix) const {
    return size_ >= suffix.size_ &&
           Equal(data_ + size_ - suffix.size_, suffix.data_, suffix.size_);
  }

  constexpr bool operator==(StringView rhs) const {
    return size_ == rhs.size_ && Equal(data_, rhs.data_, size_);
  }
  constexpr bool operator!=(StringView rhs) const { return !(*this == rhs); }

  constexpr const char* begin() const { return data_; }
  constexpr const char* end() const { return data_ + size_; }

  String ToString() const { return String(data_, size_); }

 private:
  static constexpr usize Length(const char* s) {
    usize n = 0;
    while (s[n] != '\0') ++n;
    return n;
  }
  static constexpr bool Equal(const char* a, const char* b, usize n) {
    for (usize i = 0; i < n; ++i) {
      if (a[i] != b[i]) return false;
    }
    return true;
  }

  const char* data_ = "";
  usize size_ = 0;
};

}  // namespace ugui

#endif  // ULTRAGUI_CORE_STRING_VIEW_H_
