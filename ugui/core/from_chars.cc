#include <ugui/core/from_chars_compat.h>

#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__)
#include <xlocale.h>
#endif

namespace ugui {

namespace {

int DigitValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'z') return c - 'a' + 10;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
  return 99;
}

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

// Case-insensitive match of the lowercase `word` at [p, last).
bool MatchWord(const char* p, const char* last, const char* word) {
  for (; *word; ++word, ++p) {
    if (p == last || (*p | 0x20) != *word) return false;
  }
  return true;
}

// strtof in the "C" locale: the scanned span below already has the
// std::from_chars grammar, so only the decimal point could still differ.
f32 StrToFloatC(const char* str) {
#if defined(_WIN32)
  static _locale_t c_locale = _create_locale(LC_ALL, "C");
  return _strtof_l(str, nullptr, c_locale);
#else
  static locale_t c_locale =
      newlocale(LC_ALL_MASK, "C", static_cast<locale_t>(0));
  return strtof_l(str, nullptr, c_locale);
#endif
}

}  // namespace

FromCharsResult from_chars(const char* first, const char* last, u32& value,
                           int base) {
  const char* p = first;
  u32 mag = 0;
  bool overflow = false;
  for (; p != last; ++p) {
    const int d = DigitValue(*p);
    if (d >= base) break;
    const u32 ud = static_cast<u32>(d);
    const u32 ub = static_cast<u32>(base);
    if (overflow || mag > (0xFFFFFFFFu - ud) / ub) {
      overflow = true;
    } else {
      mag = mag * ub + ud;
    }
  }
  if (p == first) return {first, FromCharsError::kInvalidArgument};
  if (overflow) return {p, FromCharsError::kOutOfRange};
  value = mag;
  return {p, FromCharsError::kNone};
}

FromCharsResult from_chars(const char* first, const char* last, f32& value) {
  const char* p = first;
  const bool negative = p != last && *p == '-';
  if (negative) ++p;

  // inf, infinity and nan[(chars)]: std::from_chars yields the plain quiet NaN
  // whatever the payload, where strtof would encode it.
  if (MatchWord(p, last, "inf")) {
    p += MatchWord(p, last, "infinity") ? 8 : 3;
    value = negative ? -INFINITY : INFINITY;
    return {p, FromCharsError::kNone};
  }
  if (MatchWord(p, last, "nan")) {
    p += 3;
    if (p != last && *p == '(') {
      const char* q = p + 1;
      while (q != last && (IsDigit(*q) || (*q >= 'a' && *q <= 'z') ||
                           (*q >= 'A' && *q <= 'Z') || *q == '_')) {
        ++q;
      }
      if (q != last && *q == ')') p = q + 1;
    }
    value = negative ? -NAN : NAN;
    return {p, FromCharsError::kNone};
  }

  // digits [. digits] [e [+-] digits], at least one mantissa digit. An
  // exponent without digits is not part of the number. Scanning the grammar
  // here, not in strtof, is what keeps out whitespace, '+' and hex floats.
  bool nonzero = false;
  const char* mantissa = p;
  while (p != last && IsDigit(*p)) nonzero |= *p++ != '0';
  usize digits = static_cast<usize>(p - mantissa);
  if (p != last && *p == '.') {
    const char* frac = ++p;
    while (p != last && IsDigit(*p)) nonzero |= *p++ != '0';
    digits += static_cast<usize>(p - frac);
  }
  if (digits == 0) return {first, FromCharsError::kInvalidArgument};
  if (p != last && (*p == 'e' || *p == 'E')) {
    const char* q = p + 1;
    if (q != last && (*q == '+' || *q == '-')) ++q;
    if (q != last && IsDigit(*q)) {
      while (q != last && IsDigit(*q)) ++q;
      p = q;
    }
  }

  const usize len = static_cast<usize>(p - first);
  char stack_buf[128];
  String heap_buf;
  char* buf = stack_buf;
  if (len >= sizeof(stack_buf)) {
    heap_buf = String(first, len);
    buf = heap_buf.data();
  } else {
    memcpy(buf, first, len);
    buf[len] = '\0';
  }
  const f32 parsed = StrToFloatC(buf);

  // std::from_chars reports a result that rounds to infinity, or a nonzero
  // mantissa that rounds to zero, as out of range and keeps the old value.
  if (isinf(parsed) || (parsed == 0.0f && nonzero)) {
    return {p, FromCharsError::kOutOfRange};
  }
  value = parsed;
  return {p, FromCharsError::kNone};
}

}  // namespace ugui
