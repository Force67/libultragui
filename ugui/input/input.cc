#include <algorithm>
#include <functional>
#include <ugui/input/input.h>
#include <ugui/platform/platform.h>
#include <ugui/style/style.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>
#include <vector>

namespace ugui {

void InputRouter::Init(Platform* platform) { platform_ = platform; }

Widget* InputRouter::Resolve(WidgetId id) const {
  return registry_ ? registry_->Get(id) : nullptr;
}

static void SetHoverBit(Widget* w, bool on) {
  auto state = w->widget_state();
  if (on)
    w->set_widget_state(state | WidgetState::kHovered);
  else
    w->set_widget_state(static_cast<WidgetState>(
        static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kHovered)));
}

bool InputRouter::Process(Widget* root) {
  if (!root || !platform_) return false;
  registry_ = root->context() ? root->context()->registry : nullptr;

  auto& queue = platform_->input_queue();
  bool consumed = false;

  // Stale handles (e.g. after a tree rebuild) resolve to null automatically;
  // no explicit pointer-validation pass is needed.

  // Process mouse movement -> hover detection
  if (queue.move_count > 0) {
    Vec2 pos = queue.move_events[queue.move_count - 1].position;  // latest
    mouse_pos_ = pos;
    Widget* hovered = Resolve(hovered_);
    Widget* new_hover = root->HitTest(pos);

    if (new_hover != hovered) {
      if (hovered) {
        SetHoverBit(hovered, false);
        if (on_hover_) on_hover_(hovered, false);
      }
      hovered_ = new_hover ? new_hover->handle() : kNullWidget;
      if (new_hover) {
        SetHoverBit(new_hover, true);
        if (on_hover_) on_hover_(new_hover, true);
        platform_->SetCursor(new_hover->ComputedStyle().cursor);
      } else {
        platform_->SetCursor(Cursor::kAuto);
      }
      consumed = true;
    }

    // Drag detection: only enter drag mode if there's a draggable target
    // reachable from the pressed widget, otherwise leave drag_target_ null so
    // the release path still fires a click.
    Widget* pressed = Resolve(pressed_);
    if (pressed) {
      Vec2 diff = {pos.x - drag_start_.x, pos.y - drag_start_.y};
      f32 dist2 = diff.x * diff.x + diff.y * diff.y;
      if (!dragging_ && dist2 > kDragThreshold * kDragThreshold) {
        // If the pressed widget (or an ancestor) is a drag handle, climb to
        // the nearest draggable ancestor so "drag the header moves the panel".
        Widget* dt = nullptr;
        Widget* w = pressed;
        while (w && !w->drag_handle()) w = w->parent();
        if (w) {
          Widget* anc = w->parent();
          while (anc && !anc->draggable()) anc = anc->parent();
          if (anc)
            dt = anc;
          else if (w->draggable())
            dt = w;
        } else if (pressed->draggable()) {
          dt = pressed;
        }
        if (dt) {
          dragging_ = true;
          drag_target_ = dt->handle();
          dt->OnDragStart(drag_start_);
        }
      }
      Widget* drag_target = Resolve(drag_target_);
      if (dragging_ && drag_target) {
        Vec2 delta = {pos.x - drag_prev_.x, pos.y - drag_prev_.y};
        drag_target->OnDragMove(pos, delta);
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
      pressed_ = target ? target->handle() : kNullWidget;
      drag_start_ = evt.position;
      drag_prev_ = evt.position;
      dragging_ = false;
      drag_target_ = kNullWidget;
      if (target) {
        target->set_widget_state(target->widget_state() |
                                 WidgetState::kPressed);
      }
      set_focus(target);
      consumed = true;
    } else {
      // Release
      Widget* drag_target = Resolve(drag_target_);
      if (dragging_ && drag_target) drag_target->OnDragEnd(evt.position);
      dragging_ = false;
      bool was_dragging = (drag_target != nullptr);
      drag_target_ = kNullWidget;
      Widget* pressed = Resolve(pressed_);
      if (pressed) {
        auto state = pressed->widget_state();
        pressed->set_widget_state(
            static_cast<WidgetState>(static_cast<u16>(state) &
                                     ~static_cast<u16>(WidgetState::kPressed)));

        // Click: press and release on same widget (only if not dragging)
        if (!was_dragging && pressed == target && target) {
          if (on_click_) on_click_(target, evt.button);
          target->OnClick();
        }
      }
      pressed_ = kNullWidget;
      consumed = true;
    }
  }

  // Process scroll -> bubble up to find a handler
  for (u32 i = 0; i < queue.scroll_count; ++i) {
    auto& evt = queue.scroll_events[i];
    Widget* w = root->HitTest(evt.position);
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
      Vector<Widget*> focusable;
      Function<void(Widget*)> collect = [&](Widget* w) {
        if (w->focusable()) focusable.push_back(w);
        for (auto* child : w->children()) collect(child);
      };
      collect(root);
      if (focusable.empty()) continue;
      std::sort(focusable.begin(), focusable.end(), [](Widget* a, Widget* b) {
        return a->tab_index() < b->tab_index();
      });
      auto it =
          std::find(focusable.begin(), focusable.end(), Resolve(focused_));
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
      if (handled) continue;

      // Enter or Space activates the focused widget (keyboard/gamepad nav),
      // unless it consumes text input (then Space is a literal character).
      Widget* focused = Resolve(focused_);
      if (focused && !focused->consumes_text_input() &&
          (evt.key == 257 /* GLFW_KEY_ENTER */ ||
           evt.key == 335 /* GLFW_KEY_KP_ENTER */ ||
           evt.key == 32 /* GLFW_KEY_SPACE */)) {
        if (on_click_) on_click_(focused, MouseButton::kLeft);
        focused->OnClick();
        consumed = true;
        continue;
      }
    }
    // Dispatch to focused widget
    if (Widget* focused = Resolve(focused_)) {
      if (evt.pressed || evt.repeat)
        consumed |= focused->OnKeyDown(evt.key, evt.mods);
      else
        consumed |= focused->OnKeyUp(evt.key, evt.mods);
    }
  }

  // Dispatch character input to focused widget
  for (u32 i = 0; i < queue.char_count; ++i) {
    if (Widget* focused = Resolve(focused_))
      consumed |= focused->OnCharInput(queue.char_events[i].codepoint);
  }

  // Process gamepad button events
  for (u32 i = 0; i < queue.gamepad_button_count; ++i) {
    auto& evt = queue.gamepad_button_events[i];
    if (!evt.pressed) continue;

    gamepad_nav_active_ = true;

    if (evt.button == GamepadButton::kA) {
      if (Widget* focused = Resolve(focused_)) {
        if (on_click_) on_click_(focused, MouseButton::kLeft);
        focused->OnClick();
        consumed = true;
      }
    } else if (evt.button == GamepadButton::kB) {
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
  f32 stick_x = 0.0f, stick_y = 0.0f;
  for (u32 i = 0; i < queue.gamepad_axis_count; ++i) {
    auto& evt = queue.gamepad_axis_events[i];
    if (evt.axis == GamepadAxis::kLeftX)
      stick_x = evt.value;
    else if (evt.axis == GamepadAxis::kLeftY)
      stick_y = evt.value;
  }
  if (stick_x != 0.0f || stick_y != 0.0f) gamepad_nav_active_ = true;

  constexpr f32 kStickNavThreshold = 0.5f;
  i8 nav_x = (stick_x > kStickNavThreshold)    ? 1
             : (stick_x < -kStickNavThreshold) ? -1
                                               : 0;
  i8 nav_y = (stick_y > kStickNavThreshold)    ? 1
             : (stick_y < -kStickNavThreshold) ? -1
                                               : 0;

  if (nav_x != gamepad_nav_dir_x_ || nav_y != gamepad_nav_dir_y_) {
    gamepad_nav_dir_x_ = nav_x;
    gamepad_nav_dir_y_ = nav_y;
    gamepad_nav_timer_ = kGamepadNavInitialDelay;
    if (nav_x != 0 || nav_y != 0) {
      NavigateFocus(root, nav_x, nav_y);
      consumed = true;
    }
  } else if (nav_x != 0 || nav_y != 0) {
    gamepad_nav_timer_ -= 1.0f / 60.0f;
    if (gamepad_nav_timer_ <= 0.0f) {
      gamepad_nav_timer_ = kGamepadNavRepeatRate;
      NavigateFocus(root, nav_x, nav_y);
      consumed = true;
    }
  }

  if (queue.move_count > 0 || queue.button_count > 0) {
    gamepad_nav_active_ = false;
  }

  queue.clear();
  return consumed;
}

void InputRouter::RefreshHover(Widget* root) {
  if (!root || !platform_) return;
  registry_ = root->context() ? root->context()->registry : nullptr;
  Widget* hovered = Resolve(hovered_);
  Widget* new_hover = root->HitTest(mouse_pos_);
  if (new_hover == hovered) return;
  if (hovered) {
    SetHoverBit(hovered, false);
    if (on_hover_) on_hover_(hovered, false);
  }
  hovered_ = new_hover ? new_hover->handle() : kNullWidget;
  if (new_hover) {
    SetHoverBit(new_hover, true);
    if (on_hover_) on_hover_(new_hover, true);
    platform_->SetCursor(new_hover->ComputedStyle().cursor);
  } else {
    platform_->SetCursor(Cursor::kAuto);
  }
}

void InputRouter::set_hover(Widget* widget) {
  if (widget && widget->context() && !registry_)
    registry_ = widget->context()->registry;
  Widget* hovered = Resolve(hovered_);
  if (hovered == widget) return;
  if (hovered) {
    SetHoverBit(hovered, false);
    if (on_hover_) on_hover_(hovered, false);
  }
  hovered_ = widget ? widget->handle() : kNullWidget;
  if (widget) {
    SetHoverBit(widget, true);
    if (on_hover_) on_hover_(widget, true);
    if (platform_) platform_->SetCursor(widget->ComputedStyle().cursor);
  } else if (platform_) {
    platform_->SetCursor(Cursor::kAuto);
  }
}

void InputRouter::set_focus(Widget* widget) {
  if (widget && widget->context() && !registry_)
    registry_ = widget->context()->registry;
  Widget* focused = Resolve(focused_);
  if (focused == widget) return;
  if (focused) {
    auto state = focused->widget_state();
    focused->set_widget_state(static_cast<WidgetState>(
        static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kFocused)));
  }
  focused_ = widget ? widget->handle() : kNullWidget;
  if (widget) {
    widget->set_widget_state(widget->widget_state() | WidgetState::kFocused);
  }
}

void InputRouter::ResetState() {
  if (Widget* w = Resolve(hovered_)) SetHoverBit(w, false);
  if (Widget* w = Resolve(pressed_)) {
    auto state = w->widget_state();
    w->set_widget_state(static_cast<WidgetState>(
        static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kPressed)));
  }
  if (Widget* w = Resolve(focused_)) {
    auto state = w->widget_state();
    w->set_widget_state(static_cast<WidgetState>(
        static_cast<u16>(state) & ~static_cast<u16>(WidgetState::kFocused)));
  }

  hovered_ = kNullWidget;
  focused_ = kNullWidget;
  pressed_ = kNullWidget;
  drag_target_ = kNullWidget;
  dragging_ = false;
  mouse_pos_ = Vec2::Zero();
  drag_start_ = Vec2::Zero();
  drag_prev_ = Vec2::Zero();

  if (platform_) platform_->SetCursor(Cursor::kAuto);
}

void InputRouter::RegisterShortcut(i32 key, i32 mods, ShortcutHandler handler) {
  shortcuts_.push_back({key, mods, std::move(handler)});
}

void InputRouter::ClearShortcuts() { shortcuts_.clear(); }

void InputRouter::ProcessGamepadNavigation(Widget* root, f32 /*delta_time*/) {
  (void)root;
}

void InputRouter::NavigateFocus(Widget* root, i8 dir_x, i8 dir_y) {
  if (!root) return;

  Vector<Widget*> focusable;
  Function<void(Widget*)> collect = [&](Widget* w) {
    if (w->focusable()) focusable.push_back(w);
    for (auto* child : w->children()) collect(child);
  };
  collect(root);
  if (focusable.empty()) return;

  std::sort(focusable.begin(), focusable.end(), [](Widget* a, Widget* b) {
    return a->tab_index() < b->tab_index();
  });

  Widget* focused = Resolve(focused_);
  if (!focused) {
    set_focus(focusable.front());
    if (on_hover_) on_hover_(focusable.front(), true);
    return;
  }

  if (dir_y != 0) {
    auto it = std::find(focusable.begin(), focusable.end(), focused);
    if (dir_y > 0) {
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

  if (dir_x != 0) {
    auto it = std::find(focusable.begin(), focusable.end(), focused);
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

  if (Widget* nf = Resolve(focused_)) {
    if (on_hover_) on_hover_(nf, true);
  }
}

}  // namespace ugui
