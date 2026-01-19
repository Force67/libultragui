#pragma once

#include <ultragui/core/string_view.h>
#include <ultragui/core/types.h>

namespace ugui {

/// Parse hex color string ("#RRGGBB" or "#RRGGBBAA") into a u32
bool parse_hex_color(StringView str, u32& out);

bool starts_with(StringView str, StringView prefix);
bool ends_with(StringView str, StringView suffix);

} // namespace ugui
