#ifndef ULTRAGUI_CORE_COLOR_H_
#define ULTRAGUI_CORE_COLOR_H_

#include <ultragui/core/math.h>
#include <ultragui/core/types.h>

namespace ugui {

struct Color {
  f32 r = 0.0f;
  f32 g = 0.0f;
  f32 b = 0.0f;
  f32 a = 1.0f;

  constexpr Color() = default;
  constexpr Color(f32 r, f32 g, f32 b, f32 a = 1.0f) : r(r), g(g), b(b), a(a) {}

  constexpr Vec4 ToVec4() const { return {r, g, b, a}; }

  static constexpr Color FromVec4(Vec4 v) { return {v.x, v.y, v.z, v.w}; }

  /// Construct from 0xRRGGBB or 0xRRGGBBAA
  static constexpr Color FromHex(u32 hex, f32 alpha = 1.0f) {
    return {
        static_cast<f32>((hex >> 16) & 0xFF) / 255.0f,
        static_cast<f32>((hex >> 8) & 0xFF) / 255.0f,
        static_cast<f32>(hex & 0xFF) / 255.0f,
        alpha,
    };
  }

  static constexpr Color FromRgba8(u8 r, u8 g, u8 b, u8 a = 255) {
    return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
  }

  constexpr Color WithAlpha(f32 alpha) const { return {r, g, b, alpha}; }

  constexpr bool operator==(const Color& rhs) const {
    return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a;
  }
  constexpr bool operator!=(const Color& rhs) const { return !(*this == rhs); }

  // Common colors
  static constexpr Color White() { return {1, 1, 1, 1}; }
  static constexpr Color Black() { return {0, 0, 0, 1}; }
  static constexpr Color Transparent() { return {0, 0, 0, 0}; }
  static constexpr Color Red() { return {1, 0, 0, 1}; }
  static constexpr Color Green() { return {0, 1, 0, 1}; }
  static constexpr Color Blue() { return {0, 0, 1, 1}; }
};

constexpr Color Lerp(Color a, Color b, f32 t) {
  return {Lerp(a.r, b.r, t), Lerp(a.g, b.g, t), Lerp(a.b, b.b, t),
          Lerp(a.a, b.a, t)};
}

}  // namespace ugui

#endif  // ULTRAGUI_CORE_COLOR_H_
