// Copyright (C) Force67 <github.com/Force67>.
// For licensing information see LICENSE at the root of this distribution.
#pragma once

// std::from_chars without <charconv>, for both container configurations. The
// parsers accept exactly what std::from_chars accepts and produce the same
// values: no leading whitespace, no '+', no "0x" prefix, '-' only for floats.
// On a range error the value is left untouched and ptr points past the
// number; when nothing matches, ptr == first.

#include <ugui/core/types.h>

namespace ugui {

enum class FromCharsError : u8 { kNone, kInvalidArgument, kOutOfRange };

struct FromCharsResult {
  const char* ptr;
  FromCharsError ec;
};

/// Unsigned integers in `base` (2..36), as std::from_chars(first, last,
/// value, base).
FromCharsResult from_chars(const char* first, const char* last, u32& value,
                           int base = 10);

/// Floats in the general (fixed or scientific) format, as
/// std::from_chars(first, last, value): correctly rounded, decimal point '.'
/// whatever the process locale, and inf/infinity/nan/nan(...) in any case.
FromCharsResult from_chars(const char* first, const char* last, f32& value);

}  // namespace ugui
