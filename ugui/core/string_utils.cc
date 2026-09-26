#include <ugui/core/string_utils.h>
#include <ugui/core/from_chars_compat.h>

namespace ugui {

bool ParseHexColor(StringView str, u32& out) {
  if (str.empty()) return false;

  const char* begin = str.data();
  usize len = str.size();

  // Skip leading '#'
  if (begin[0] == '#') {
    begin++;
    len--;
  }

  if (len != 6 && len != 8) return false;

  auto result = ugui::from_chars(begin, begin + len, out, 16);
  return result.ec == FromCharsError::kNone;
}

bool starts_with(StringView str, StringView prefix) {
  return str.starts_with(prefix);
}

bool ends_with(StringView str, StringView suffix) {
  return str.ends_with(suffix);
}

}  // namespace ugui
