#include <ultragui/input/input.h>
#include <ultragui/platform/platform.h>
#include <ultragui/style/style.h>
#include <ultragui/widgets/widget.h>

#include <algorithm>
#include <functional>
#include <vector>

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

        // Drag detection: if mouse is pressed and moved beyond threshold
        if (pressed_) {
            Vec2 diff = {pos.x - drag_start_.x, pos.y - drag_start_.y};
            f32 dist2 = diff.x * diff.x + diff.y * diff.y;
            if (!dragging_ && dist2 > kDragThreshold * kDragThreshold) {
                dragging_ = true;
                pressed_->OnDragStart(drag_start_);
            }
            if (dragging_) {
                Vec2 delta = {pos.x - drag_prev_.x, pos.y - drag_prev_.y};
                pressed_->OnDragMove(pos, delta);
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
            if (target) {
                target->set_widget_state(target->widget_state() | WidgetState::kPressed);
            }
            // Update focus
            set_focus(target);
            consumed = true;
        } else {
            // Release
            if (dragging_ && pressed_) {
                pressed_->OnDragEnd(evt.position);
                dragging_ = false;
            }
            if (pressed_) {
                auto state = pressed_->widget_state();
                pressed_->set_widget_state(static_cast<WidgetState>(
                    static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kPressed)));

                // Click: press and release on same widget (only if not dragging)
                if (!dragging_ && pressed_ == target && target) {
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
            std::vector<Widget*> focusable;
            std::function<void(Widget*)> collect = [&](Widget* w) {
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

void InputRouter::RegisterShortcut(i32 key, i32 mods, ShortcutHandler handler) {
    shortcuts_.push_back({key, mods, std::move(handler)});
}

void InputRouter::ClearShortcuts() {
    shortcuts_.clear();
}

} // namespace ugui
