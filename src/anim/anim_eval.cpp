#include <ultragui/anim/anim_types.h>
#include <ultragui/style/transition.h>

#include <algorithm>
#include <cmath>

namespace ugui {

// ---------------------------------------------------------------------------
// Interpolation helpers
// ---------------------------------------------------------------------------

static f32 lerp_f32(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

static Color lerp_color(Color a, Color b, f32 t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

// ---------------------------------------------------------------------------
// Evaluate a single property track at time t
// ---------------------------------------------------------------------------

static f32 eval_scalar_track(const AnimPropertyTrack& track, f32 t) {
    auto& kfs = track.keyframes;
    if (kfs.empty()) return 0;
    if (kfs.size() == 1) return kfs[0].value;

    // Before first keyframe
    if (t <= kfs.front().t) return kfs.front().value;
    // After last keyframe
    if (t >= kfs.back().t) return kfs.back().value;

    // Find bracketing keyframes
    for (usize i = 0; i + 1 < kfs.size(); ++i) {
        if (t >= kfs[i].t && t <= kfs[i + 1].t) {
            f32 segment_t = (kfs[i + 1].t - kfs[i].t) > 0
                                ? (t - kfs[i].t) / (kfs[i + 1].t - kfs[i].t)
                                : 1.0f;
            f32 eased = eval_easing(kfs[i].easing, segment_t);
            return lerp_f32(kfs[i].value, kfs[i + 1].value, eased);
        }
    }
    return kfs.back().value;
}

static Color eval_color_track(const AnimPropertyTrack& track, f32 t) {
    auto& kfs = track.keyframes;
    if (kfs.empty()) return Color::transparent();
    if (kfs.size() == 1) return kfs[0].color;

    if (t <= kfs.front().t) return kfs.front().color;
    if (t >= kfs.back().t) return kfs.back().color;

    for (usize i = 0; i + 1 < kfs.size(); ++i) {
        if (t >= kfs[i].t && t <= kfs[i + 1].t) {
            f32 segment_t = (kfs[i + 1].t - kfs[i].t) > 0
                                ? (t - kfs[i].t) / (kfs[i + 1].t - kfs[i].t)
                                : 1.0f;
            f32 eased = eval_easing(kfs[i].easing, segment_t);
            return lerp_color(kfs[i].color, kfs[i + 1].color, eased);
        }
    }
    return kfs.back().color;
}

// ---------------------------------------------------------------------------
// Evaluate a full layer
// ---------------------------------------------------------------------------

void evaluate_layer(const AnimLayer& layer, f32 normalized_t, EvaluatedLayer& out) {
    // Start with static defaults
    out.x = layer.x;
    out.y = layer.y;
    out.w = layer.w;
    out.h = layer.h;
    out.r = layer.r;
    out.cx = layer.cx;
    out.cy = layer.cy;
    out.rx = layer.rx;
    out.ry = layer.ry;
    out.corner_radius = layer.corner_radius;
    out.rotation = layer.rotation;
    out.scale_x = layer.scale_x;
    out.scale_y = layer.scale_y;
    out.opacity = layer.opacity;
    out.stroke_width = layer.stroke_width;
    out.fill_animated = false;
    out.stroke_animated = false;

    // Override with animated values
    for (auto& track : layer.tracks) {
        bool is_color = (track.property == AnimProperty::Fill ||
                         track.property == AnimProperty::Stroke);
        if (is_color) {
            Color c = eval_color_track(track, normalized_t);
            if (track.property == AnimProperty::Fill) {
                out.fill_color = c;
                out.fill_animated = true;
            } else {
                out.stroke_color = c;
                out.stroke_animated = true;
            }
        } else {
            f32 v = eval_scalar_track(track, normalized_t);
            switch (track.property) {
            case AnimProperty::X: out.x = v; break;
            case AnimProperty::Y: out.y = v; break;
            case AnimProperty::W: out.w = v; break;
            case AnimProperty::H: out.h = v; break;
            case AnimProperty::R: out.r = v; break;
            case AnimProperty::Cx: out.cx = v; break;
            case AnimProperty::Cy: out.cy = v; break;
            case AnimProperty::Rx: out.rx = v; break;
            case AnimProperty::Ry: out.ry = v; break;
            case AnimProperty::Rotation: out.rotation = v; break;
            case AnimProperty::ScaleX: out.scale_x = v; break;
            case AnimProperty::ScaleY: out.scale_y = v; break;
            case AnimProperty::Opacity: out.opacity = v; break;
            case AnimProperty::StrokeWidth: out.stroke_width = v; break;
            case AnimProperty::CornerRadius: out.corner_radius = v; break;
            default: break;
            }
        }
    }
}

} // namespace ugui
