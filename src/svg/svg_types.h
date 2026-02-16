#ifndef SRC_SVG_SVG_TYPES_H_
#define SRC_SVG_SVG_TYPES_H_

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

    static Transform Identity() { return {1, 0, 0, 1, 0, 0}; }

    static Transform Translate(f32 tx, f32 ty) { return {1, 0, 0, 1, tx, ty}; }

    static Transform Scale(f32 sx, f32 sy) { return {sx, 0, 0, sy, 0, 0}; }

    static Transform Rotate(f32 degrees) {
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

    Vec2 Apply(Vec2 p) const { return {a * p.x + c * p.y + e, b * p.x + d * p.y + f}; }
};

// --- Path representation ---

enum class PathCmd : u8 {
    kMoveTo,  // 1 point
    kLineTo,  // 1 point
    kCubicTo, // 3 points (c1, c2, end)
    kClose,   // 0 points
};

struct PathEntry {
    PathCmd cmd;
    Vec2 pts[3];
};

struct Path {
    std::vector<PathEntry> entries;

    void MoveTo(Vec2 p) { entries.push_back({PathCmd::kMoveTo, {p, {}, {}}}); }
    void LineTo(Vec2 p) { entries.push_back({PathCmd::kLineTo, {p, {}, {}}}); }
    void CubicTo(Vec2 c1, Vec2 c2, Vec2 end) {
        entries.push_back({PathCmd::kCubicTo, {c1, c2, end}});
    }
    void Close() { entries.push_back({PathCmd::kClose, {{}, {}, {}}}); }
};

// --- Paint (fill/stroke) ---

enum class FillRule : u8 { kNonZero, kEvenOdd };

struct GradientStop {
    f32 offset;
    Color color;
};

enum class GradientType : u8 { kLinear, kRadial };
enum class SpreadMethod : u8 { kPad, kReflect, kRepeat };

struct Gradient {
    GradientType type = GradientType::kLinear;
    f32 x1 = 0, y1 = 0, x2 = 1, y2 = 0;           // linear
    f32 cx = 0.5f, cy = 0.5f, r = 0.5f;             // radial
    f32 fx = -1, fy = -1;                            // radial focus (-1 = use center)
    std::vector<GradientStop> stops;
    Transform transform = Transform::Identity();
    bool user_space = false; // true = userSpaceOnUse
    SpreadMethod spread = SpreadMethod::kPad;
};

struct Paint {
    enum Type : u8 { kNone, kSolid, kGradientRef };
    Type type = kNone;
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
    FillRule fill_rule = FillRule::kNonZero;
    Transform transform = Transform::Identity();
};

// --- Document ---

struct Document {
    f32 width = 0, height = 0;
    f32 view_x = 0, view_y = 0, view_w = 0, view_h = 0;
    std::vector<Shape> shapes;
    std::unordered_map<std::string, Gradient> gradients;
};

// Internal API
bool ParseSvg(const char* data, usize length, Document& out);
void Rasterize(const Document& doc, u8* pixels, u32 width, u32 height);
void ParsePathData(const char* d, Path& path);

} // namespace svg
} // namespace ugui

#endif  // SRC_SVG_SVG_TYPES_H_
