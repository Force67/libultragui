#include <ultragui/anim/anim_types.h>
#include <ultragui/anim/json.h>
#include <ultragui/core/color.h>

#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace ugui {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Color parse_anim_color(const char* s) {
    if (!s || !*s) return Color::transparent();
    if (s[0] == '#') {
        u32 hex = 0;
        usize len = std::strlen(s);
        std::from_chars(s + 1, s + len, hex, 16);
        if (len == 7) return Color::from_hex(hex);
        if (len == 9) {
            u32 rgb = hex >> 8;
            f32 alpha = static_cast<f32>(hex & 0xFF) / 255.0f;
            return Color::from_hex(rgb, alpha);
        }
    }
    if (std::strcmp(s, "none") == 0 || std::strcmp(s, "transparent") == 0)
        return Color::transparent();
    if (std::strcmp(s, "white") == 0) return Color::white();
    if (std::strcmp(s, "black") == 0) return Color::black();
    return Color::transparent();
}

static EasingType parse_anim_easing(const char* s) {
    if (!s) return EasingType::Linear;
    if (std::strcmp(s, "ease-in-out") == 0) return EasingType::EaseInOut;
    if (std::strcmp(s, "ease-in") == 0) return EasingType::EaseIn;
    if (std::strcmp(s, "ease-out") == 0) return EasingType::EaseOut;
    return EasingType::Linear;
}

static AnimShapeType parse_shape_type(const char* s) {
    if (!s) return AnimShapeType::Rect;
    if (std::strcmp(s, "rect") == 0) return AnimShapeType::Rect;
    if (std::strcmp(s, "circle") == 0) return AnimShapeType::Circle;
    if (std::strcmp(s, "ellipse") == 0) return AnimShapeType::Ellipse;
    if (std::strcmp(s, "path") == 0) return AnimShapeType::Path;
    if (std::strcmp(s, "group") == 0) return AnimShapeType::Group;
    return AnimShapeType::Rect;
}

static AnimProperty parse_property_name(const char* s) {
    if (std::strcmp(s, "x") == 0) return AnimProperty::X;
    if (std::strcmp(s, "y") == 0) return AnimProperty::Y;
    if (std::strcmp(s, "w") == 0 || std::strcmp(s, "width") == 0) return AnimProperty::W;
    if (std::strcmp(s, "h") == 0 || std::strcmp(s, "height") == 0) return AnimProperty::H;
    if (std::strcmp(s, "r") == 0) return AnimProperty::R;
    if (std::strcmp(s, "cx") == 0) return AnimProperty::Cx;
    if (std::strcmp(s, "cy") == 0) return AnimProperty::Cy;
    if (std::strcmp(s, "rx") == 0) return AnimProperty::Rx;
    if (std::strcmp(s, "ry") == 0) return AnimProperty::Ry;
    if (std::strcmp(s, "rotation") == 0) return AnimProperty::Rotation;
    if (std::strcmp(s, "scale-x") == 0) return AnimProperty::ScaleX;
    if (std::strcmp(s, "scale-y") == 0) return AnimProperty::ScaleY;
    if (std::strcmp(s, "opacity") == 0) return AnimProperty::Opacity;
    if (std::strcmp(s, "stroke-width") == 0) return AnimProperty::StrokeWidth;
    if (std::strcmp(s, "corner-radius") == 0) return AnimProperty::CornerRadius;
    if (std::strcmp(s, "fill") == 0) return AnimProperty::Fill;
    if (std::strcmp(s, "stroke") == 0) return AnimProperty::Stroke;
    return AnimProperty::Opacity; // fallback
}

static bool is_color_property(AnimProperty p) {
    return p == AnimProperty::Fill || p == AnimProperty::Stroke;
}

// ---------------------------------------------------------------------------
// Parse keyframes
// ---------------------------------------------------------------------------

static AnimPropertyTrack parse_track(const char* prop_name, const JsonValue& arr) {
    AnimPropertyTrack track;
    track.property = parse_property_name(prop_name);
    bool is_color = is_color_property(track.property);

    for (auto& kf_val : arr.array_val) {
        AnimKeyframe kf;
        if (auto* t = kf_val.get("t")) kf.t = t->as_float();
        if (is_color) {
            if (auto* v = kf_val.get("v")) kf.color = parse_anim_color(v->as_string());
        } else {
            if (auto* v = kf_val.get("v")) kf.value = v->as_float();
        }
        if (auto* e = kf_val.get("ease")) kf.easing = parse_anim_easing(e->as_string());
        track.keyframes.push_back(kf);
    }
    return track;
}

// ---------------------------------------------------------------------------
// Parse layer
// ---------------------------------------------------------------------------

static AnimLayer parse_layer(const JsonValue& obj) {
    AnimLayer layer;

    if (auto* v = obj.get("name")) layer.name = v->as_string();
    if (auto* v = obj.get("shape")) layer.shape_type = parse_shape_type(v->as_string());

    // Static properties
    if (auto* v = obj.get("x")) layer.x = v->as_float();
    if (auto* v = obj.get("y")) layer.y = v->as_float();
    if (auto* v = obj.get("w")) layer.w = v->as_float();
    if (auto* v = obj.get("h")) layer.h = v->as_float();
    if (auto* v = obj.get("r")) layer.r = v->as_float();
    if (auto* v = obj.get("cx")) layer.cx = v->as_float();
    if (auto* v = obj.get("cy")) layer.cy = v->as_float();
    if (auto* v = obj.get("rx")) layer.rx = v->as_float();
    if (auto* v = obj.get("ry")) layer.ry = v->as_float();
    if (auto* v = obj.get("corner-radius")) layer.corner_radius = v->as_float();
    if (auto* v = obj.get("rotation")) layer.rotation = v->as_float();
    if (auto* v = obj.get("scale-x")) layer.scale_x = v->as_float(1);
    if (auto* v = obj.get("scale-y")) layer.scale_y = v->as_float(1);
    if (auto* v = obj.get("opacity")) layer.opacity = v->as_float(1);
    if (auto* v = obj.get("stroke-width")) layer.stroke_width = v->as_float(1);

    if (auto* v = obj.get("d")) layer.path_data = v->as_string();
    if (auto* v = obj.get("fill")) layer.fill_str = v->as_string("none");
    if (auto* v = obj.get("stroke")) layer.stroke_str = v->as_string("none");
    if (auto* v = obj.get("mask")) layer.mask_layer = v->as_string();

    // Keyframes
    if (auto* kf = obj.get("keyframes")) {
        for (usize i = 0; i < kf->object_keys.size(); ++i) {
            if (kf->object_vals[i].type == JsonValue::Array) {
                layer.tracks.push_back(parse_track(kf->object_keys[i].c_str(), kf->object_vals[i]));
            }
        }
    }

    // Children (for groups)
    if (auto* children = obj.get("children")) {
        for (auto& child : children->array_val) {
            layer.children.push_back(parse_layer(child));
        }
    }

    return layer;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool parse_anim_document(const JsonValue& root, AnimDocument& out) {
    if (root.type != JsonValue::Object) return false;

    if (auto* v = root.get("version")) out.version = static_cast<u32>(v->as_number(1));
    if (auto* v = root.get("width")) out.width = v->as_float();
    if (auto* v = root.get("height")) out.height = v->as_float();
    if (auto* v = root.get("duration")) out.duration = v->as_float(1);
    if (auto* v = root.get("loop")) out.loop = v->as_bool(false);

    // Layers
    if (auto* layers = root.get("layers")) {
        for (auto& layer_val : layers->array_val) {
            out.layers.push_back(parse_layer(layer_val));
        }
    }

    return !out.layers.empty();
}

bool parse_anim_file(const char* path, AnimDocument& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::fprintf(stderr, "ultragui/anim: failed to open '%s'\n", path);
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string data = ss.str();

    JsonValue root;
    if (!parse_json(data.c_str(), data.size(), root)) {
        std::fprintf(stderr, "ultragui/anim: JSON parse error in '%s'\n", path);
        return false;
    }

    return parse_anim_document(root, out);
}

bool parse_anim_data(const char* json_data, usize length, AnimDocument& out) {
    JsonValue root;
    if (!parse_json(json_data, length, root)) return false;
    return parse_anim_document(root, out);
}

} // namespace ugui
