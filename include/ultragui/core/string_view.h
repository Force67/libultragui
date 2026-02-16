#ifndef ULTRAGUI_CORE_STRING_VIEW_H_
#define ULTRAGUI_CORE_STRING_VIEW_H_

#include <ultragui/core/types.h>

#include <cstring>
#include <string_view>

namespace ugui {

/// Non-owning string reference. Thin wrapper around std::string_view
/// with convenience methods for the library's needs.
class StringView {
public:
    constexpr StringView() = default;
    constexpr StringView(const char* str) : sv_(str ? str : "") {}
    constexpr StringView(const char* str, usize len) : sv_(str, len) {}
    constexpr StringView(std::string_view sv) : sv_(sv) {}

    constexpr const char* data() const { return sv_.data(); }
    constexpr usize size() const { return sv_.size(); }
    constexpr bool empty() const { return sv_.empty(); }

    constexpr char operator[](usize i) const { return sv_[i]; }

    constexpr StringView substr(usize pos, usize count = std::string_view::npos) const {
        return sv_.substr(pos, count);
    }

    constexpr bool starts_with(StringView prefix) const { return sv_.starts_with(prefix.sv_); }
    constexpr bool ends_with(StringView suffix) const { return sv_.ends_with(suffix.sv_); }

    constexpr bool operator==(StringView rhs) const { return sv_ == rhs.sv_; }
    constexpr bool operator!=(StringView rhs) const { return sv_ != rhs.sv_; }

    constexpr operator std::string_view() const { return sv_; }

    constexpr auto begin() const { return sv_.begin(); }
    constexpr auto end() const { return sv_.end(); }

private:
    std::string_view sv_;
};

} // namespace ugui

#endif  // ULTRAGUI_CORE_STRING_VIEW_H_
