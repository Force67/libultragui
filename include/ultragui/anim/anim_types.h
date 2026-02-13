#pragma once

#include <ultragui/core/color.h>
#include <ultragui/core/types.h>
#include <ultragui/style/transition.h>

#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations for SVG types used by the renderer
namespace ugui { namespace svg {
struct Path;
struct Gradient;
struct Paint;
}} // namespace ugui::svg

namespace ugui {

/// Property types that can be animated in a .uganim layer.
enum class AnimProperty : u8 {
    X, Y, W, H,
    R, Cx, Cy, Rx, Ry,
    Rotation, ScaleX, ScaleY,
    Opacity, StrokeWidth, CornerRadius,
    Fill, Stroke, // color properties
};

/// A single keyframe: time + value + easing.
struct AnimKeyframe {
    f32 t = 0;      // normalized time (0.0-1.0 within duration)
    f32 value = 0;   // for scalar properties
    Color color;     // for color properties (Fill, Stroke)
    EasingType easing = EasingType::Linear;
};

/// A track of keyframes for one property.
struct AnimPropertyTrack {
    AnimProperty property;
    std::vector<AnimKeyframe> keyframes;
};

/// Shape types supported in .uganim.
enum class AnimShapeType : u8 {
    Rect,
    Circle,
    Ellipse,
    Path,
    Group,
};

/// A layer in the animation. Each layer has a shape, static defaults,
/// keyframed property tracks, and optional children (for groups).
struct AnimLayer {
    std::string name;
    AnimShapeType shape_type = AnimShapeType::Rect;

    // Static shape parameters (defaults, overridden by keyframes)
    f32 x = 0, y = 0, w = 0, h = 0;
    f32 r = 0, cx = 0, cy = 0, rx = 0, ry = 0;
    f32 corner_radius = 0;
    f32 rotation = 0, scale_x = 1, scale_y = 1;
    f32 opacity = 1;
    f32 stroke_width = 1;

    // Path data (for shape_type == Path)
    std::string path_data; // SVG path "d" attribute string

    // Fill and stroke (as color strings or "$gradient_name")
    std::string fill_str = "none";
    std::string stroke_str = "none";

    // Mask: name of another layer to clip by
    std::string mask_layer;

    // Animated property tracks
    std::vector<AnimPropertyTrack> tracks;

    // Children (for groups)
    std::vector<AnimLayer> children;
};

/// Evaluated layer state at a specific point in time.
struct EvaluatedLayer {
    f32 x, y, w, h;
    f32 r, cx, cy, rx, ry;
    f32 corner_radius;
    f32 rotation, scale_x, scale_y;
    f32 opacity, stroke_width;
    Color fill_color;
    Color stroke_color;
    bool fill_animated = false;
    bool stroke_animated = false;
};

/// Top-level animation document parsed from .uganim JSON.
struct AnimDocument {
    u32 version = 1;
    f32 width = 0, height = 0;
    f32 duration = 1.0f;
    bool loop = false;

    std::unordered_map<std::string, std::string> gradients_json; // raw gradient data for svg
    std::vector<AnimLayer> layers;
};

} // namespace ugui
