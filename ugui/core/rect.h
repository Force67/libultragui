#ifndef ULTRAGUI_CORE_RECT_H_
#define ULTRAGUI_CORE_RECT_H_

#include <ugui/core/math.h>

namespace ugui {

struct Rect {
  f32 x = 0.0f;
  f32 y = 0.0f;
  f32 w = 0.0f;
  f32 h = 0.0f;

  constexpr Rect() = default;
  constexpr Rect(f32 x, f32 y, f32 w, f32 h) : x(x), y(y), w(w), h(h) {}
  constexpr Rect(Vec2 pos, Vec2 size)
      : x(pos.x), y(pos.y), w(size.x), h(size.y) {}

  constexpr Vec2 pos() const { return {x, y}; }
  constexpr Vec2 size() const { return {w, h}; }
  constexpr Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }

  constexpr f32 left() const { return x; }
  constexpr f32 right() const { return x + w; }
  constexpr f32 top() const { return y; }
  constexpr f32 bottom() const { return y + h; }

  constexpr Vec2 top_left() const { return {x, y}; }
  constexpr Vec2 top_right() const { return {x + w, y}; }
  constexpr Vec2 bottom_left() const { return {x, y + h}; }
  constexpr Vec2 bottom_right() const { return {x + w, y + h}; }

  constexpr bool contains(Vec2 p) const {
    return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
  }

  constexpr bool Intersects(Rect other) const {
    return x < other.x + other.w && x + w > other.x && y < other.y + other.h &&
           y + h > other.y;
  }

  constexpr Rect Intersected(Rect other) const {
    f32 ix = x > other.x ? x : other.x;
    f32 iy = y > other.y ? y : other.y;
    f32 ir = (x + w < other.x + other.w) ? x + w : other.x + other.w;
    f32 ib = (y + h < other.y + other.h) ? y + h : other.y + other.h;
    f32 iw = ir - ix;
    f32 ih = ib - iy;
    if (iw <= 0.0f || ih <= 0.0f) return {};
    return {ix, iy, iw, ih};
  }

  /// Expand rect by `amount` on each side
  constexpr Rect Expanded(f32 amount) const {
    return {x - amount, y - amount, w + amount * 2, h + amount * 2};
  }

  /// Shrink rect by `amount` on each side
  constexpr Rect Shrunk(f32 amount) const { return Expanded(-amount); }

  constexpr Rect Translated(Vec2 offset) const {
    return {x + offset.x, y + offset.y, w, h};
  }

  constexpr bool operator==(Rect rhs) const {
    return x == rhs.x && y == rhs.y && w == rhs.w && h == rhs.h;
  }
  constexpr bool operator!=(Rect rhs) const { return !(*this == rhs); }
};

/// Edge insets (padding, margin, border)
struct EdgeInsets {
  f32 top = 0.0f;
  f32 right = 0.0f;
  f32 bottom = 0.0f;
  f32 left = 0.0f;

  constexpr EdgeInsets() = default;
  constexpr explicit EdgeInsets(f32 all)
      : top(all), right(all), bottom(all), left(all) {}
  constexpr EdgeInsets(f32 vertical, f32 horizontal)
      : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
  constexpr EdgeInsets(f32 top, f32 right, f32 bottom, f32 left)
      : top(top), right(right), bottom(bottom), left(left) {}

  constexpr f32 horizontal() const { return left + right; }
  constexpr f32 vertical() const { return top + bottom; }

  constexpr bool operator==(const EdgeInsets& rhs) const {
    return top == rhs.top && right == rhs.right && bottom == rhs.bottom &&
           left == rhs.left;
  }
};

}  // namespace ugui

#endif  // ULTRAGUI_CORE_RECT_H_
