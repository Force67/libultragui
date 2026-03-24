#ifndef ULTRAGUI_CORE_STRING_UTILS_H_
#define ULTRAGUI_CORE_STRING_UTILS_H_

#include <ugui/core/string_view.h>
#include <ugui/core/types.h>

namespace ugui {

/// Parse hex color string ("#RRGGBB" or "#RRGGBBAA") into a u32
bool ParseHexColor(StringView str, u32& out);

bool starts_with(StringView str, StringView prefix);
bool ends_with(StringView str, StringView suffix);

}  // namespace ugui

#endif  // ULTRAGUI_CORE_STRING_UTILS_H_
