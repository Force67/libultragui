#include <ultragui/input/input.h>
#include <ultragui/platform/platform.h>
#include <ultragui/style/style.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

void InputRouter::Init(Platform* platform) {
    platform_ = platform;
}

bool InputRouter::Process(Widget* root) {
    if (!root || !platform_)
        return false;

    auto& queue = platform_->input_queue();
    bool consumed = false;

    // Process mouse movement -> hover detection
    if (queue.move_count > 0) {
        Vec2 pos = queue.move_events[queue.move_count - 1].position; // Use latest position
        mouse_pos_ = pos;
        Widget* new_hover = root->HitTest(pos);

        if (new_hover != hovered_) {
            if (hovered_) {
                auto state = hovered_->widget_state();
                hovered_->set_widget_state(static_cast<WidgetState>(
                    static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kHovered)));
                if (on_hover_)
                    on_hover_(hovered_, false);
            }
            hovered_ = new_hover;
            if (hovered_) {
                hovered_->set_widget_state(hovered_->widget_state() | WidgetState::kHovered);
                if (on_hover_)
                    on_hover_(hovered_, true);

                // Update cursor based on hovered widget's style
                Cursor cur = hovered_->ComputedStyle().cursor;
                platform_->SetCursor(cur);
            } else {
                platform_->SetCursor(Cursor::kAuto);
            }
            consumed = true;
        }
    }

    // Process mouse buttons -> press/click
    for (u32 i = 0; i < queue.button_count; ++i) {
        auto& evt = queue.button_events[i];
        Widget* target = root->HitTest(evt.position);

        if (evt.pressed) {
            pressed_ = target;
            if (target) {
                target->set_widget_state(target->widget_state() | WidgetState::kPressed);
            }
            // Update focus
            set_focus(target);
            consumed = true;
        } else {
            // Release
            if (pressed_) {
                auto state = pressed_->widget_state();
                pressed_->set_widget_state(static_cast<WidgetState>(
                    static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kPressed)));

                // Click: press and release on same widget
                if (pressed_ == target && target) {
                    if (on_click_)
                        on_click_(target, evt.button);

                    // Dispatch via virtual method (no dynamic_cast needed)
                    target->OnClick();
                }
            }
            pressed_ = nullptr;
            consumed = true;
        }
    }

    // Process scroll -> bubble up to find a handler
    for (u32 i = 0; i < queue.scroll_count; ++i) {
        auto& evt = queue.scroll_events[i];
        Widget* target = root->HitTest(evt.position);

        // Walk up the tree to find a widget that handles scroll
        Widget* w = target;
        while (w) {
            if (w->OnScroll(Vec2{-evt.delta.x * 40.0f, -evt.delta.y * 40.0f})) {
                consumed = true;
                break;
            }
            w = w->parent();
        }
    }

    return consumed;
}

void InputRouter::set_focus(Widget* widget) {
    if (focused_ == widget)
        return;

    if (focused_) {
        auto state = focused_->widget_state();
        focused_->set_widget_state(static_cast<WidgetState>(
            static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kFocused)));
    }
    focused_ = widget;
    if (focused_) {
        focused_->set_widget_state(focused_->widget_state() | WidgetState::kFocused);
    }
}

} // namespace ugui
