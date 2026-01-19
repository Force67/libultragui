#pragma once

#include <ultragui/core/types.h>

namespace ugui {

/// Per-vertex data for 2D UI rendering.
/// The fragment shader uses half_size + corner_radius to compute an SDF
/// for crisp rounded rectangles at any resolution.
struct Vertex2D {
    f32 pos[2]; // Screen-space position (pixels)
    f32 uv[2];  // Texture UV (0-1)
    u32 color;  // Packed ABGR8
    f32 corner_radius;
    f32 half_size[2]; // Rect half-extents for SDF (0,0 = no SDF)

    static constexpr u32 pack_color(f32 r, f32 g, f32 b, f32 a) {
        auto to_u8 = [](f32 v) -> u32 { return static_cast<u32>(v * 255.0f + 0.5f); };
        return to_u8(a) << 24 | to_u8(b) << 16 | to_u8(g) << 8 | to_u8(r);
    }
};

static_assert(sizeof(Vertex2D) == 32, "Vertex2D must be 32 bytes");

} // namespace ugui
