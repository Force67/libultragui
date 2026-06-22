// Copyright (C) Force67 <github.com/Force67>.
// For licensing information see LICENSE at the root of this distribution.
#pragma once

// Apple's libc++ implements std::from_chars for integral types but leaves the
// floating-point overloads deleted, so float parsing routes through strtod here
// while integers still go straight to std::from_chars. Every other platform
// forwards both to the standard library unchanged.

#include <charconv>
#include <cstdlib>
#include <cstring>
#include <system_error>
#include <type_traits>

namespace ugui {

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline std::from_chars_result from_chars(const char* first, const char* last,
                                         T& value, int base = 10) {
  return std::from_chars(first, last, value, base);
}

template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
inline std::from_chars_result from_chars(const char* first, const char* last,
                                         T& value) {
#if defined(__APPLE__)
  char buf[64];
  size_t n = static_cast<size_t>(last - first);
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  std::memcpy(buf, first, n);
  buf[n] = '\0';
  char* end = nullptr;
  const double parsed = std::strtod(buf, &end);
  std::from_chars_result result{};
  result.ptr = first + (end - buf);
  result.ec = (end == buf) ? std::errc::invalid_argument : std::errc{};
  value = static_cast<T>(parsed);
  return result;
#else
  return std::from_chars(first, last, value);
#endif
}

}  // namespace ugui
