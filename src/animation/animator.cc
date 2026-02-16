#include <ultragui/animation/animator.h>

#include <algorithm>

namespace ugui {

// ---------------------------------------------------------------------------
// KeyframeAnimation
// ---------------------------------------------------------------------------

Style KeyframeAnimation::Evaluate(f64 current_time, bool& done) const {
    f64 elapsed = current_time - start_time - delay;
    if (elapsed < 0.0 || keyframes.size() < 2) {
        done = false;
        return keyframes.empty() ? Style{} : keyframes.front().style;
    }

    f32 total_elapsed = static_cast<f32>(elapsed);
    i32 iteration = static_cast<i32>(total_elapsed / duration);

    if (repeat_count >= 0 && iteration >= repeat_count) {
        done = true;
        return keyframes.back().style;
    }

    f32 local_t = (total_elapsed - iteration * duration) / duration;
    local_t = std::clamp(local_t, 0.0f, 1.0f);

    if (alternate && (iteration % 2 == 1)) {
        local_t = 1.0f - local_t;
    }

    f32 eased = EvalEasing(easing, local_t);

    // Find the two keyframes to interpolate between
    usize lo = 0;
    for (usize i = 1; i < keyframes.size(); ++i) {
        if (keyframes[i].time >= eased) {
            lo = i - 1;
            break;
        }
        lo = i;
    }
    usize hi = std::min(lo + 1, keyframes.size() - 1);

    if (lo == hi) {
        done = false;
        return keyframes[lo].style;
    }

    f32 segment_t = (eased - keyframes[lo].time) / (keyframes[hi].time - keyframes[lo].time);
    segment_t = std::clamp(segment_t, 0.0f, 1.0f);

    done = false;
    return Style::Lerp(keyframes[lo].style, keyframes[hi].style, segment_t);
}

// ---------------------------------------------------------------------------
// Animator
// ---------------------------------------------------------------------------

void Animator::StartTransition(u32 widget_id, const Style& from, const Style& to,
                                const Transition& transition, f64 current_time) {
    // Cancel existing transition for this widget
    for (auto& t : transitions_) {
        if (t.widget_id == widget_id)
            t.active = false;
    }

    transitions_.push_back({widget_id, from, to, transition, current_time, true});
}

void Animator::StartAnimation(const KeyframeAnimation& anim, f64 current_time) {
    auto copy = anim;
    copy.start_time = current_time;
    copy.active = true;
    animations_.push_back(std::move(copy));
}

void Animator::Cancel(u32 widget_id) {
    for (auto& t : transitions_) {
        if (t.widget_id == widget_id)
            t.active = false;
    }
    for (auto& a : animations_) {
        if (a.widget_id == widget_id)
            a.active = false;
    }
}

bool Animator::Update(f64 current_time, ApplyFn apply, void* user_data) {
    bool any_active = false;

    for (auto& t : transitions_) {
        if (!t.active)
            continue;
        bool done = false;
        Style s = t.Evaluate(current_time, done);
        apply(t.widget_id, s, user_data);
        if (done)
            t.active = false;
        else
            any_active = true;
    }

    for (auto& a : animations_) {
        if (!a.active)
            continue;
        bool done = false;
        Style s = a.Evaluate(current_time, done);
        apply(a.widget_id, s, user_data);
        if (done)
            a.active = false;
        else
            any_active = true;
    }

    // Compact: remove inactive entries
    transitions_.erase(std::remove_if(transitions_.begin(), transitions_.end(),
                                      [](const StyleTransition& t) { return !t.active; }),
                       transitions_.end());
    animations_.erase(std::remove_if(animations_.begin(), animations_.end(),
                                     [](const KeyframeAnimation& a) { return !a.active; }),
                      animations_.end());

    return any_active;
}

bool Animator::IsAnimating(u32 widget_id) const {
    for (auto& t : transitions_) {
        if (t.widget_id == widget_id && t.active)
            return true;
    }
    for (auto& a : animations_) {
        if (a.widget_id == widget_id && a.active)
            return true;
    }
    return false;
}

} // namespace ugui
