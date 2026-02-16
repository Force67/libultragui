#ifndef ULTRAGUI_ANIMATION_ANIMATOR_H_
#define ULTRAGUI_ANIMATION_ANIMATOR_H_

#include <ultragui/core/types.h>
#include <ultragui/style/style.h>
#include <ultragui/style/transition.h>

#include <vector>

namespace ugui {

/// Animates a single style property transition between two Style states.
struct StyleTransition {
    u32 widget_id = 0;
    Style from;
    Style to;
    Transition transition;
    f64 start_time = 0.0;
    bool active = false;

    /// Returns interpolated style at current time. Sets `done` if finished.
    Style Evaluate(f64 current_time, bool& done) const {
        f64 elapsed = current_time - start_time - transition.delay;
        if (elapsed < 0.0) {
            done = false;
            return from;
        }
        f32 t =
            (transition.duration > 0.0f) ? static_cast<f32>(elapsed / transition.duration) : 1.0f;
        if (t >= 1.0f) {
            done = true;
            return to;
        }
        done = false;
        f32 eased = EvalEasing(transition.easing, t, transition.bezier);
        return Style::Lerp(from, to, eased);
    }
};

/// Keyframe in a multi-stop animation
struct Keyframe {
    f32 time; // 0.0 to 1.0 (normalized within total duration)
    Style style;
};

/// Multi-keyframe animation sequence
struct KeyframeAnimation {
    u32 widget_id = 0;
    std::vector<Keyframe> keyframes;
    f32 duration = 1.0f; // total seconds
    f32 delay = 0.0f;
    EasingType easing = EasingType::kLinear;
    i32 repeat_count = 1;   // -1 = infinite
    bool alternate = false; // ping-pong
    f64 start_time = 0.0;
    bool active = false;

    Style Evaluate(f64 current_time, bool& done) const;
};

/// Manages active transitions and animations for the UI.
class Animator {
public:
    /// Start a property transition for a widget
    void StartTransition(u32 widget_id, const Style& from, const Style& to,
                          const Transition& transition, f64 current_time);

    /// Start a keyframe animation
    void StartAnimation(const KeyframeAnimation& anim, f64 current_time);

    /// Cancel all animations for a widget
    void Cancel(u32 widget_id);

    /// Tick all animations, apply to styles via the callback.
    /// Returns true if any animations are still active.
    using ApplyFn = void (*)(u32 widget_id, const Style& animated_style, void* user_data);
    bool Update(f64 current_time, ApplyFn apply, void* user_data);

    /// Check if a widget has active animations
    bool IsAnimating(u32 widget_id) const;

private:
    std::vector<StyleTransition> transitions_;
    std::vector<KeyframeAnimation> animations_;
};

} // namespace ugui

#endif  // ULTRAGUI_ANIMATION_ANIMATOR_H_
