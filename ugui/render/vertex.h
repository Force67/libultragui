#ifndef ULTRAGUI_RENDER_VERTEX_H_
#define ULTRAGUI_RENDER_VERTEX_H_

#include <ugui/core/types.h>

namespace ugui {

/// Per-vertex data for 2D UI rendering.
/// The fragment shader uses half_size + corner_radii to compute an SDF
/// for crisp rounded rectangles at any resolution.
/// color2 enables linear gradients; softness controls shadow blur.
struct Vertex2D {
  f32 pos[2];        // Screen-space position (pixels)
  f32 uv[2];         // Texture UV (0-1)
  u32 color;         // Packed ABGR8 (primary / gradient start)
  u32 color2;        // Packed ABGR8 (gradient end, or same as color for solid)
  u32 corner_radii;  // Packed: TL | (TR << 8) | (BR << 16) | (BL << 24)
  f32 softness;      // Shadow blur (0 = crisp edge, >0 = soft falloff)
  f32 half_size[2];  // Rect half-extents for SDF (0,0 = no SDF)
  f32 border_width;  // Border thickness (0 = no border)
  u32 border_color;  // Packed ABGR8

  static constexpr u32 PackColor(f32 r, f32 g, f32 b, f32 a) {
    auto to_u8 = [](f32 v) -> u32 {
      return static_cast<u32>(v * 255.0f + 0.5f);
    };
    return to_u8(a) << 24 | to_u8(b) << 16 | to_u8(g) << 8 | to_u8(r);
  }

  /// Pack 4 per-corner radii (TL, TR, BR, BL) into a single u32.
  static constexpr u32 PackRadii(f32 tl, f32 tr, f32 br, f32 bl) {
    auto to_u8 = [](f32 v) -> u32 { return static_cast<u32>(v + 0.5f) & 0xFF; };
    return to_u8(tl) | (to_u8(tr) << 8) | (to_u8(br) << 16) | (to_u8(bl) << 24);
  }

  /// Convenience: pack a uniform radius for all 4 corners.
  static constexpr u32 PackRadii(f32 r) { return PackRadii(r, r, r, r); }
};

static_assert(sizeof(Vertex2D) == 48, "Vertex2D must be 48 bytes");

}  // namespace ugui

#endif  // ULTRAGUI_RENDER_VERTEX_H_
