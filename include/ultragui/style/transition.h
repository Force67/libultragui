#ifndef ULTRAGUI_STYLE_TRANSITION_H_
#define ULTRAGUI_STYLE_TRANSITION_H_

#include <ultragui/core/types.h>

namespace ugui {

/// Easing function type for property transitions and animations.
enum class EasingType : u8 {
    kLinear,
    kEaseIn,
    kEaseOut,
    kEaseInOut,
    kCubicBezier,
};

/// Describes how a property transitions between values.
struct Transition {
    f32 duration = 0.0f; // seconds
    f32 delay = 0.0f;    // seconds
    EasingType easing = EasingType::kEaseInOut;
    // For CubicBezier: control points
    f32 bezier[4] = {0.25f, 0.1f, 0.25f, 1.0f};

    constexpr bool IsInstant() const { return duration <= 0.0f; }
};

/// Evaluate an easing curve at time t (0-1), returns 0-1
f32 EvalEasing(EasingType type, f32 t, const f32 bezier[4] = nullptr);

} // namespace ugui

#endif  // ULTRAGUI_STYLE_TRANSITION_H_
