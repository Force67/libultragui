#pragma once

#include <ultragui/core/types.h>

namespace ugui {

/// Easing function type for property transitions and animations.
enum class EasingType : u8 {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    CubicBezier,
};

/// Describes how a property transitions between values.
struct Transition {
    f32 duration = 0.0f; // seconds
    f32 delay = 0.0f;    // seconds
    EasingType easing = EasingType::EaseInOut;
    // For CubicBezier: control points
    f32 bezier[4] = {0.25f, 0.1f, 0.25f, 1.0f};

    constexpr bool is_instant() const { return duration <= 0.0f; }
};

/// Evaluate an easing curve at time t (0-1), returns 0-1
f32 eval_easing(EasingType type, f32 t, const f32 bezier[4] = nullptr);

} // namespace ugui
