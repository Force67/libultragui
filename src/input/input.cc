#include <ultragui/input/input.h>
#include <ultragui/platform/platform.h>
#include <ultragui/style/style.h>
#include <ultragui/widgets/widget.h>

#include <algorithm>
#include <functional>
#include <vector>

namespace ugui {

static bool IsInWidgetTree(const Widget* root, const Widget* target) {
    if (!root || !target)
        return false;
    if (root == target)
        return true;
    for (auto* child : root->children()) {
        if (IsInWidgetTree(child, target))
            return true;
    }
    return false;
}

void InputRouter::Init(Platform* platform) {
    platform_ = platform;
}

bool InputRouter::Process(Widget* root) {
    if (!root || !platform_)
        return false;

    auto& queue = platform_->input_queue();
    bool consumed = false;

    // Root may be replaced between frames. Drop stale cached pointers before
    // any dereference.
    if (hovered_ && !IsInWidgetTree(root, hovered_)) {
        hovered_ = nullptr;
        platform_->SetCursor(Cursor::kAuto);
    }
    if (pressed_ && !IsInWidgetTree(root, pressed_)) {
        pressed_ = nullptr;
        dragging_ = false;
        drag_target_ = nullptr;
    }
    if (drag_target_ && !IsInWidgetTree(root, drag_target_)) {
        drag_target_ = nullptr;
        dragging_ = false;
    }
    if (focused_ && !IsInWidgetTree(root, focused_)) {
        focused_ = nullptr;
    }

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

        // Drag detection: if mouse is pressed and moved beyond threshold,
        // AND there's a draggable target reachable from the pressed
        // widget, enter drag mode. Otherwise leave drag_target_ null so
        // the release path still fires a click - without this guard, any
        // tiny mouse drift between press and release on a non-draggable
        // widget (button, category row, etc.) would set drag_target_ to
        // the pressed widget, the default OnDragStart would no-op, and
        // the release would then suppress the click via was_dragging.
        if (pressed_) {
            Vec2 diff = {pos.x - drag_start_.x, pos.y - drag_start_.y};
            f32 dist2 = diff.x * diff.x + diff.y * diff.y;
            if (!dragging_ && dist2 > kDragThreshold * kDragThreshold) {
                // Pick the actual drag target. If the pressed widget (or
                // any ancestor up to the first non-handle) is tagged as
                // a drag handle, walk further up to find the nearest
                // draggable ancestor and dispatch drag events there.
                // This is what makes "click panel header -> drag panel"
                // work without the header swallowing button clicks.
                Widget* dt = nullptr;
                Widget* w = pressed_;
                while (w && !w->drag_handle())
                    w = w->parent();
                if (w) {
                    // Found a handle - climb to its draggable ancestor.
                    Widget* anc = w->parent();
                    while (anc && !anc->draggable())
                        anc = anc->parent();
                    if (anc)
                        dt = anc;
                    else if (w->draggable())
                        dt = w;
                } else if (pressed_->draggable()) {
                    // Pressed widget is itself draggable (no handle needed).
                    dt = pressed_;
                }
                if (dt) {
                    dragging_ = true;
                    drag_target_ = dt;
                    drag_target_->OnDragStart(drag_start_);
                }
            }
            if (dragging_ && drag_target_) {
                Vec2 delta = {pos.x - drag_prev_.x, pos.y - drag_prev_.y};
                drag_target_->OnDragMove(pos, delta);
                drag_prev_ = pos;
                consumed = true;
            }
        }
    }

    // Process mouse buttons -> press/click
    for (u32 i = 0; i < queue.button_count; ++i) {
        auto& evt = queue.button_events[i];
        Widget* target = root->HitTest(evt.position);

        if (evt.pressed) {
            pressed_ = target;
            drag_start_ = evt.position;
            drag_prev_ = evt.position;
            dragging_ = false;
            drag_target_ = nullptr;
            if (target) {
                target->set_widget_state(target->widget_state() | WidgetState::kPressed);
            }
            // Update focus
            set_focus(target);
            consumed = true;
        } else {
            // Release
            if (dragging_ && drag_target_) {
                drag_target_->OnDragEnd(evt.position);
            }
            dragging_ = false;
            bool was_dragging = (drag_target_ != nullptr);
            drag_target_ = nullptr;
            if (pressed_) {
                auto state = pressed_->widget_state();
                pressed_->set_widget_state(static_cast<WidgetState>(
                    static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kPressed)));

                // Click: press and release on same widget (only if not dragging)
                if (!was_dragging && pressed_ == target && target) {
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

    // Tab navigation
    for (u32 i = 0; i < queue.key_count; ++i) {
        auto& evt = queue.key_events[i];
        if (evt.pressed && evt.key == 258 /* GLFW_KEY_TAB */) {
            bool reverse = (evt.mods & 0x0001 /* GLFW_MOD_SHIFT */) != 0;
            // Collect focusable widgets
            Vector<Widget*> focusable;
            Function<void(Widget*)> collect = [&](Widget* w) {
                if (w->focusable())
                    focusable.push_back(w);
                for (auto* child : w->children())
                    collect(child);
            };
            collect(root);
            if (focusable.empty())
                continue;
            // Sort by tab_index
            std::sort(focusable.begin(), focusable.end(),
                      [](Widget* a, Widget* b) { return a->tab_index() < b->tab_index(); });
            // Find current focused widget's position
            auto it = std::find(focusable.begin(), focusable.end(), focused_);
            if (reverse) {
                if (it == focusable.begin() || it == focusable.end())
                    set_focus(focusable.back());
                else
                    set_focus(*std::prev(it));
            } else {
                if (it == focusable.end() || std::next(it) == focusable.end())
                    set_focus(focusable.front());
                else
                    set_focus(*std::next(it));
            }
            consumed = true;
        }
    }

    // Check global shortcuts before dispatching to focused widget
    for (u32 i = 0; i < queue.key_count; ++i) {
        auto& evt = queue.key_events[i];
        if (evt.pressed) {
            bool handled = false;
            for (auto& sc : shortcuts_) {
                if (evt.key == sc.key && (evt.mods & sc.mods) == sc.mods) {
                    sc.handler();
                    consumed = true;
                    handled = true;
                    break;
                }
            }
            if (handled)
                continue;

            // Enter or Space activates the focused widget (keyboard/gamepad
            // nav). Skipped when the focused widget is a text input - for
            // those, Space is a literal character and Enter is up to the
            // widget's own OnKeyDown to interpret (e.g. submit). Otherwise
            // pressing space inside a TextInput would invoke OnClick which
            // re-seats the caret at the last mouse position before the
            // OnCharInput pass inserts the literal space.
            if (focused_ && !focused_->consumes_text_input() &&
                (evt.key == 257 /* GLFW_KEY_ENTER */ ||
                 evt.key == 335 /* GLFW_KEY_KP_ENTER */ ||
                 evt.key == 32  /* GLFW_KEY_SPACE */)) {
                if (on_click_)
                    on_click_(focused_, MouseButton::kLeft);
                focused_->OnClick();
                consumed = true;
                continue;
            }
        }
        // Dispatch to focused widget
        if (focused_) {
            if (evt.pressed || evt.repeat)
                consumed |= focused_->OnKeyDown(evt.key, evt.mods);
            else
                consumed |= focused_->OnKeyUp(evt.key, evt.mods);
        }
    }

    // Dispatch character input to focused widget
    for (u32 i = 0; i < queue.char_count; ++i) {
        if (focused_)
            consumed |= focused_->OnCharInput(queue.char_events[i].codepoint);
    }

    // Process gamepad button events
    for (u32 i = 0; i < queue.gamepad_button_count; ++i) {
        auto& evt = queue.gamepad_button_events[i];
        if (!evt.pressed) continue;

        gamepad_nav_active_ = true;

        if (evt.button == GamepadButton::kA) {
            // A button = confirm / activate focused widget
            if (focused_) {
                if (on_click_)
                    on_click_(focused_, MouseButton::kLeft);
                focused_->OnClick();
                consumed = true;
            }
        } else if (evt.button == GamepadButton::kB) {
            // B button = back / cancel
            if (on_gamepad_back_) {
                on_gamepad_back_();
                consumed = true;
            }
        } else if (evt.button == GamepadButton::kDPadUp) {
            NavigateFocus(root, 0, -1);
            consumed = true;
        } else if (evt.button == GamepadButton::kDPadDown) {
            NavigateFocus(root, 0, 1);
            consumed = true;
        } else if (evt.button == GamepadButton::kDPadLeft) {
            NavigateFocus(root, -1, 0);
            consumed = true;
        } else if (evt.button == GamepadButton::kDPadRight) {
            NavigateFocus(root, 1, 0);
            consumed = true;
        }
    }

    // Gamepad left stick navigation (with repeat)
    // Accumulate the latest axis state from events
    f32 stick_x = 0.0f, stick_y = 0.0f;
    for (u32 i = 0; i < queue.gamepad_axis_count; ++i) {
        auto& evt = queue.gamepad_axis_events[i];
        if (evt.axis == GamepadAxis::kLeftX) stick_x = evt.value;
        else if (evt.axis == GamepadAxis::kLeftY) stick_y = evt.value;
    }
    if (stick_x != 0.0f || stick_y != 0.0f) {
        gamepad_nav_active_ = true;
    }

    // Determine navigation direction from stick
    constexpr f32 kStickNavThreshold = 0.5f;
    i8 nav_x = (stick_x > kStickNavThreshold) ? 1 : (stick_x < -kStickNavThreshold) ? -1 : 0;
    i8 nav_y = (stick_y > kStickNavThreshold) ? 1 : (stick_y < -kStickNavThreshold) ? -1 : 0;

    if (nav_x != gamepad_nav_dir_x_ || nav_y != gamepad_nav_dir_y_) {
        // Direction changed - immediate navigation
        gamepad_nav_dir_x_ = nav_x;
        gamepad_nav_dir_y_ = nav_y;
        gamepad_nav_timer_ = kGamepadNavInitialDelay;
        if (nav_x != 0 || nav_y != 0) {
            NavigateFocus(root, nav_x, nav_y);
            consumed = true;
        }
    } else if (nav_x != 0 || nav_y != 0) {
        // Same direction held - repeat after delay
        // Note: we use a rough 1/60 estimate since we don't have delta_time here
        gamepad_nav_timer_ -= 1.0f / 60.0f;
        if (gamepad_nav_timer_ <= 0.0f) {
            gamepad_nav_timer_ = kGamepadNavRepeatRate;
            NavigateFocus(root, nav_x, nav_y);
            consumed = true;
        }
    }

    // Mouse input switches back to mouse mode
    if (queue.move_count > 0 || queue.button_count > 0) {
        gamepad_nav_active_ = false;
    }

    // Clear consumed events so they don't reprocess next frame.
    // The built-in GLFW platform clears in PollEvents(), but external
    // integrations (like nextrym) push events outside that path.
    queue.clear();

    return consumed;
}

void InputRouter::RefreshHover(Widget* root) {
    if (!root || !platform_)
        return;
    Widget* new_hover = root->HitTest(mouse_pos_);
    if (new_hover == hovered_)
        return;
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
        platform_->SetCursor(hovered_->ComputedStyle().cursor);
    } else {
        platform_->SetCursor(Cursor::kAuto);
    }
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

void InputRouter::ResetState() {
    if (hovered_) {
        auto state = hovered_->widget_state();
        hovered_->set_widget_state(static_cast<WidgetState>(
            static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kHovered)));
    }
    if (pressed_) {
        auto state = pressed_->widget_state();
        pressed_->set_widget_state(static_cast<WidgetState>(
            static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kPressed)));
    }
    if (focused_) {
        auto state = focused_->widget_state();
        focused_->set_widget_state(static_cast<WidgetState>(
            static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kFocused)));
    }

    hovered_ = nullptr;
    focused_ = nullptr;
    pressed_ = nullptr;
    drag_target_ = nullptr;
    dragging_ = false;
    mouse_pos_ = Vec2::Zero();
    drag_start_ = Vec2::Zero();
    drag_prev_ = Vec2::Zero();

    if (platform_)
        platform_->SetCursor(Cursor::kAuto);
}

void InputRouter::RegisterShortcut(i32 key, i32 mods, ShortcutHandler handler) {
    shortcuts_.push_back({key, mods, std::move(handler)});
}

void InputRouter::ClearShortcuts() {
    shortcuts_.clear();
}

void InputRouter::ProcessGamepadNavigation(Widget* root, f32 /*delta_time*/) {
    // Handled inline in Process() via gamepad button/axis events.
    (void)root;
}

void InputRouter::NavigateFocus(Widget* root, i8 dir_x, i8 dir_y) {
    if (!root) return;

    // Collect all focusable widgets
    Vector<Widget*> focusable;
    Function<void(Widget*)> collect = [&](Widget* w) {
        if (w->focusable())
            focusable.push_back(w);
        for (auto* child : w->children())
            collect(child);
    };
    collect(root);
    if (focusable.empty()) return;

    // Sort by tab_index for sequential navigation
    std::sort(focusable.begin(), focusable.end(),
              [](Widget* a, Widget* b) { return a->tab_index() < b->tab_index(); });

    if (!focused_) {
        // No focus yet - focus the first widget
        set_focus(focusable.front());
        if (on_hover_ && focused_) on_hover_(focused_, true);
        return;
    }

    // For vertical D-pad (up/down), navigate sequentially through tab order
    if (dir_y != 0) {
        auto it = std::find(focusable.begin(), focusable.end(), focused_);
        if (dir_y > 0) {
            // Down: next in tab order
            if (it == focusable.end() || std::next(it) == focusable.end())
                set_focus(focusable.front());
            else
                set_focus(*std::next(it));
        } else {
            // Up: previous in tab order
            if (it == focusable.begin() || it == focusable.end())
                set_focus(focusable.back());
            else
                set_focus(*std::prev(it));
        }
    }

    // For horizontal D-pad (left/right), also navigate sequentially (for menus)
    if (dir_x != 0) {
        auto it = std::find(focusable.begin(), focusable.end(), focused_);
        if (dir_x > 0) {
            if (it == focusable.end() || std::next(it) == focusable.end())
                set_focus(focusable.front());
            else
                set_focus(*std::next(it));
        } else {
            if (it == focusable.begin() || it == focusable.end())
                set_focus(focusable.back());
            else
                set_focus(*std::prev(it));
        }
    }

    // Trigger hover callback so the UI plays sounds on focus change
    if (on_hover_ && focused_) {
        on_hover_(focused_, true);
    }
}

} // namespace ugui
