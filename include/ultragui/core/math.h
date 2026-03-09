#ifndef ULTRAGUI_CORE_MATH_H_
#define ULTRAGUI_CORE_MATH_H_

#include <ultragui/core/types.h>

#include <cmath>

namespace ugui {

struct Vec2 {
  f32 x = 0.0f;
  f32 y = 0.0f;

  constexpr Vec2() = default;
  constexpr Vec2(f32 x, f32 y) : x(x), y(y) {}
  constexpr explicit Vec2(f32 v) : x(v), y(v) {}

  constexpr Vec2 operator+(Vec2 rhs) const { return {x + rhs.x, y + rhs.y}; }
  constexpr Vec2 operator-(Vec2 rhs) const { return {x - rhs.x, y - rhs.y}; }
  constexpr Vec2 operator*(f32 s) const { return {x * s, y * s}; }
  constexpr Vec2 operator/(f32 s) const { return {x / s, y / s}; }

  constexpr Vec2& operator+=(Vec2 rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }
  constexpr Vec2& operator-=(Vec2 rhs) {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }
  constexpr Vec2& operator*=(f32 s) {
    x *= s;
    y *= s;
    return *this;
  }

  constexpr f32 Dot(Vec2 rhs) const { return x * rhs.x + y * rhs.y; }
  f32 length() const { return std::sqrt(x * x + y * y); }
  constexpr f32 LengthSq() const { return x * x + y * y; }

  Vec2 Normalized() const {
    f32 len = length();
    return len > 0.0f ? Vec2{x / len, y / len} : Vec2{0.0f, 0.0f};
  }

  constexpr bool operator==(Vec2 rhs) const { return x == rhs.x && y == rhs.y; }
  constexpr bool operator!=(Vec2 rhs) const { return !(*this == rhs); }

  static constexpr Vec2 Zero() { return {0.0f, 0.0f}; }
  static constexpr Vec2 One() { return {1.0f, 1.0f}; }
};

constexpr Vec2 operator*(f32 s, Vec2 v) { return {s * v.x, s * v.y}; }

struct Vec4 {
  f32 x = 0.0f;
  f32 y = 0.0f;
  f32 z = 0.0f;
  f32 w = 0.0f;

  constexpr Vec4() = default;
  constexpr Vec4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}
  constexpr explicit Vec4(f32 v) : x(v), y(v), z(v), w(v) {}

  constexpr Vec4 operator+(Vec4 rhs) const {
    return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w};
  }
  constexpr Vec4 operator-(Vec4 rhs) const {
    return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w};
  }
  constexpr Vec4 operator*(f32 s) const { return {x * s, y * s, z * s, w * s}; }

  constexpr bool operator==(Vec4 rhs) const {
    return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
  }
  constexpr bool operator!=(Vec4 rhs) const { return !(*this == rhs); }
};

constexpr f32 Lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

constexpr Vec2 Lerp(Vec2 a, Vec2 b, f32 t) {
  return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t)};
}

constexpr Vec4 Lerp(Vec4 a, Vec4 b, f32 t) {
  return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t),
          Lerp(a.w, b.w, t)};
}

constexpr f32 Clamp(f32 v, f32 lo, f32 hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace ugui

#endif  // ULTRAGUI_CORE_MATH_H_
