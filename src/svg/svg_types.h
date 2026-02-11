#pragma once

#include <ultragui/core/color.h>
#include <ultragui/core/math.h>
#include <ultragui/core/types.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace ugui {
namespace svg {

// --- 2D affine transform: [a c e; b d f; 0 0 1] ---

struct Transform {
    f32 a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;

    static Transform identity() { return {1, 0, 0, 1, 0, 0}; }

    static Transform translate(f32 tx, f32 ty) { return {1, 0, 0, 1, tx, ty}; }

    static Transform scale(f32 sx, f32 sy) { return {sx, 0, 0, sy, 0, 0}; }

    static Transform rotate(f32 degrees) {
        f32 rad = degrees * 3.14159265358979323846f / 180.0f;
        f32 cs = std::cos(rad);
        f32 sn = std::sin(rad);
        return {cs, sn, -sn, cs, 0, 0};
    }

    Transform operator*(const Transform& r) const {
        return {
            a * r.a + c * r.b,
            b * r.a + d * r.b,
            a * r.c + c * r.d,
            b * r.c + d * r.d,
            a * r.e + c * r.f + e,
            b * r.e + d * r.f + f,
        };
    }

    Vec2 apply(Vec2 p) const { return {a * p.x + c * p.y + e, b * p.x + d * p.y + f}; }
};

// --- Path representation ---

enum class PathCmd : u8 {
    MoveTo,  // 1 point
    LineTo,  // 1 point
    CubicTo, // 3 points (c1, c2, end)
    Close,   // 0 points
};

struct PathEntry {
    PathCmd cmd;
    Vec2 pts[3];
};

struct Path {
    std::vector<PathEntry> entries;

    void move_to(Vec2 p) { entries.push_back({PathCmd::MoveTo, {p, {}, {}}}); }
    void line_to(Vec2 p) { entries.push_back({PathCmd::LineTo, {p, {}, {}}}); }
    void cubic_to(Vec2 c1, Vec2 c2, Vec2 end) {
        entries.push_back({PathCmd::CubicTo, {c1, c2, end}});
    }
    void close() { entries.push_back({PathCmd::Close, {{}, {}, {}}}); }
};

// --- Paint (fill/stroke) ---

enum class FillRule : u8 { NonZero, EvenOdd };

struct GradientStop {
    f32 offset;
    Color color;
};

enum class GradientType : u8 { Linear, Radial };
enum class SpreadMethod : u8 { Pad, Reflect, Repeat };

struct Gradient {
    GradientType type = GradientType::Linear;
    f32 x1 = 0, y1 = 0, x2 = 1, y2 = 0;           // linear
    f32 cx = 0.5f, cy = 0.5f, r = 0.5f;             // radial
    f32 fx = -1, fy = -1;                            // radial focus (-1 = use center)
    std::vector<GradientStop> stops;
    Transform transform = Transform::identity();
    bool user_space = false; // true = userSpaceOnUse
    SpreadMethod spread = SpreadMethod::Pad;
};

struct Paint {
    enum Type : u8 { None, Solid, GradientRef };
    Type type = None;
    Color color;
    std::string gradient_id;
};

// --- Shape ---

struct Shape {
    Path path;
    Paint fill;
    Paint stroke;
    f32 stroke_width = 1.0f;
    f32 opacity = 1.0f;
    f32 fill_opacity = 1.0f;
    f32 stroke_opacity = 1.0f;
    FillRule fill_rule = FillRule::NonZero;
    Transform transform = Transform::identity();
};

// --- Document ---

struct Document {
    f32 width = 0, height = 0;
    f32 view_x = 0, view_y = 0, view_w = 0, view_h = 0;
    std::vector<Shape> shapes;
    std::unordered_map<std::string, Gradient> gradients;
};

// Internal API
bool parse_svg(const char* data, usize length, Document& out);
void rasterize(const Document& doc, u8* pixels, u32 width, u32 height);

} // namespace svg
} // namespace ugui
