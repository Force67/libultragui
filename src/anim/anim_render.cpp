#include <ultragui/anim/anim_types.h>
#include "../../src/svg/svg_types.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace ugui {

// Declared in anim_eval.cpp
void evaluate_layer(const AnimLayer& layer, f32 normalized_t, EvaluatedLayer& out);

// ---------------------------------------------------------------------------
// Color string -> svg::Paint
// ---------------------------------------------------------------------------

static svg::Paint make_paint(const std::string& str, Color animated_color, bool was_animated) {
    svg::Paint p;
    if (was_animated) {
        p.type = svg::Paint::Solid;
        p.color = animated_color;
        return p;
    }
    if (str.empty() || str == "none" || str == "transparent") {
        p.type = svg::Paint::None;
        return p;
    }
    if (str[0] == '$') {
        p.type = svg::Paint::GradientRef;
        p.gradient_id = str.substr(1);
        return p;
    }
    // Parse as color
    p.type = svg::Paint::Solid;
    if (str[0] == '#') {
        u32 hex = 0;
        std::from_chars(str.data() + 1, str.data() + str.size(), hex, 16);
        if (str.size() == 7) p.color = Color::from_hex(hex);
        else if (str.size() == 9) {
            u32 rgb = hex >> 8;
            f32 alpha = static_cast<f32>(hex & 0xFF) / 255.0f;
            p.color = Color::from_hex(rgb, alpha);
        }
    } else if (str == "white") p.color = Color::white();
    else if (str == "black") p.color = Color::black();
    else { p.type = svg::Paint::None; }
    return p;
}

// ---------------------------------------------------------------------------
// Build SVG shape from evaluated layer
// ---------------------------------------------------------------------------

// Kappa constant for circle-to-cubic approximation
static constexpr f32 KAPPA = 0.5522847498f;

static void build_circle_path(svg::Path& path, f32 cx, f32 cy, f32 rx, f32 ry) {
    f32 kx = rx * KAPPA;
    f32 ky = ry * KAPPA;
    path.move_to({cx, cy - ry});
    path.cubic_to({cx + kx, cy - ry}, {cx + rx, cy - ky}, {cx + rx, cy});
    path.cubic_to({cx + rx, cy + ky}, {cx + kx, cy + ry}, {cx, cy + ry});
    path.cubic_to({cx - kx, cy + ry}, {cx - rx, cy + ky}, {cx - rx, cy});
    path.cubic_to({cx - rx, cy - ky}, {cx - kx, cy - ry}, {cx, cy - ry});
    path.close();
}

static void build_rect_path(svg::Path& path, f32 x, f32 y, f32 w, f32 h, f32 cr) {
    if (cr <= 0) {
        path.move_to({x, y});
        path.line_to({x + w, y});
        path.line_to({x + w, y + h});
        path.line_to({x, y + h});
        path.close();
        return;
    }
    cr = std::min(cr, std::min(w, h) * 0.5f);
    f32 k = cr * KAPPA;
    path.move_to({x + cr, y});
    path.line_to({x + w - cr, y});
    path.cubic_to({x + w - cr + k, y}, {x + w, y + cr - k}, {x + w, y + cr});
    path.line_to({x + w, y + h - cr});
    path.cubic_to({x + w, y + h - cr + k}, {x + w - cr + k, y + h}, {x + w - cr, y + h});
    path.line_to({x + cr, y + h});
    path.cubic_to({x + cr - k, y + h}, {x, y + h - cr + k}, {x, y + h - cr});
    path.line_to({x, y + cr});
    path.cubic_to({x, y + cr - k}, {x + cr - k, y}, {x + cr, y});
    path.close();
}

// Cache parsed paths to avoid re-parsing every frame.
// Keyed by pointer to the layer's path_data string (stable for AnimDocument lifetime).
static thread_local std::unordered_map<const std::string*, svg::Path> s_path_cache;

static const svg::Path& get_cached_path(const AnimLayer& layer) {
    auto it = s_path_cache.find(&layer.path_data);
    if (it != s_path_cache.end())
        return it->second;
    auto& path = s_path_cache[&layer.path_data];
    svg::parse_path_data(layer.path_data.c_str(), path);
    return path;
}

static svg::Shape build_shape(const AnimLayer& layer, const EvaluatedLayer& eval) {
    svg::Shape shape;
    shape.opacity = eval.opacity;
    shape.fill = make_paint(layer.fill_str, eval.fill_color, eval.fill_animated);
    shape.stroke = make_paint(layer.stroke_str, eval.stroke_color, eval.stroke_animated);
    shape.stroke_width = eval.stroke_width;

    switch (layer.shape_type) {
    case AnimShapeType::Rect:
        build_rect_path(shape.path, eval.x, eval.y, eval.w, eval.h, eval.corner_radius);
        break;
    case AnimShapeType::Circle:
        build_circle_path(shape.path, eval.cx, eval.cy, eval.r, eval.r);
        break;
    case AnimShapeType::Ellipse:
        build_circle_path(shape.path, eval.cx, eval.cy, eval.rx, eval.ry);
        break;
    case AnimShapeType::Path:
        if (!layer.path_data.empty())
            shape.path = get_cached_path(layer);
        break;
    case AnimShapeType::Group:
        break;
    }

    // Apply transform: translate to center, rotate, scale
    svg::Transform t = svg::Transform::identity();
    if (eval.rotation != 0 || eval.scale_x != 1 || eval.scale_y != 1) {
        // Compute center for rotation
        f32 pivot_x = (layer.shape_type == AnimShapeType::Rect) ? eval.x + eval.w * 0.5f : eval.cx;
        f32 pivot_y = (layer.shape_type == AnimShapeType::Rect) ? eval.y + eval.h * 0.5f : eval.cy;
        t = svg::Transform::translate(pivot_x, pivot_y) *
            svg::Transform::rotate(eval.rotation) *
            svg::Transform::scale(eval.scale_x, eval.scale_y) *
            svg::Transform::translate(-pivot_x, -pivot_y);
    }
    shape.transform = t;

    return shape;
}

// ---------------------------------------------------------------------------
// Render layers recursively
// ---------------------------------------------------------------------------

static void render_layers(const std::vector<AnimLayer>& layers, f32 normalized_t,
                          svg::Document& svg_doc, svg::Transform parent_xform) {
    for (auto& layer : layers) {
        EvaluatedLayer eval;
        evaluate_layer(layer, normalized_t, eval);

        if (eval.opacity <= 0) continue;

        if (layer.shape_type == AnimShapeType::Group) {
            // Apply group transform and recurse into children
            svg::Transform group_t = parent_xform;
            if (eval.rotation != 0 || eval.scale_x != 1 || eval.scale_y != 1) {
                group_t = parent_xform *
                          svg::Transform::translate(eval.x, eval.y) *
                          svg::Transform::rotate(eval.rotation) *
                          svg::Transform::scale(eval.scale_x, eval.scale_y);
            } else if (eval.x != 0 || eval.y != 0) {
                group_t = parent_xform * svg::Transform::translate(eval.x, eval.y);
            }
            render_layers(layer.children, normalized_t, svg_doc, group_t);
        } else {
            svg::Shape shape = build_shape(layer, eval);
            // Combine with parent transform
            shape.transform = parent_xform * shape.transform;
            svg_doc.shapes.push_back(std::move(shape));
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Thread-local scratch buffers to avoid per-frame allocation
static thread_local svg::Document s_svg_doc;

void render_anim_frame(const AnimDocument& doc, f32 time, u8* pixels, u32 width, u32 height) {
    f32 normalized_t = (doc.duration > 0) ? time / doc.duration : 0;
    normalized_t = std::clamp(normalized_t, 0.0f, 1.0f);

    // Reuse scratch document (avoid reallocating the shapes vector)
    s_svg_doc.shapes.clear();
    s_svg_doc.gradients.clear();
    s_svg_doc.width = static_cast<f32>(width);
    s_svg_doc.height = static_cast<f32>(height);
    s_svg_doc.view_w = doc.width;
    s_svg_doc.view_h = doc.height;

    render_layers(doc.layers, normalized_t, s_svg_doc, svg::Transform::identity());

    // Rasterize (clears pixels internally)
    svg::rasterize(s_svg_doc, pixels, width, height);
}

} // namespace ugui
