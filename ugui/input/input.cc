#include <algorithm>
#include <functional>
#include <ugui/input/input.h>
#include <ugui/platform/platform.h>
#include <ugui/style/style.h>
#include <ugui/widgets/components.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>
#include <vector>

namespace ugui {

void InputRouter::Init(Platform* platform) { platform_ = platform; }

static void SetHoverBit(World& world, wid w, bool on) {
  auto state = WidgetStateOf(world, w);
  if (on)
    SetWidgetState(world, w, state | WidgetState::kHovered);
  else
    SetWidgetState(world, w,
                   static_cast<WidgetState>(static_cast<u16>(state) &
                                            ~static_cast<u16>(
                                                WidgetState::kHovered)));
}

// --- Drag-to-move system ----------------------------------------------------
// The Movable component carries the drag state; these run when a press on a
// Movable (or its DragHandle) crosses the drag threshold. The first move pins
// the widget to its current rect via style.left/top in pixels (clearing any
// right/bottom anchoring), then each move offsets that anchor by cursor-press.

static void DragStart(World& world, wid w, Vec2 pos) {
  Movable* m = world.Get<Movable>(w);
  if (!m) return;
  Rect r = world.Get<Transform>(w)->rect;
  m->origin_x = r.x;
  m->origin_y = r.y;
  m->press = pos;
  Style& s = world.Get<StyleC>(w)->style;
  s.left_offset = Length::Px(r.x);
  s.top = Length::Px(r.y);
  s.right_offset = Length::Auto();
  s.bottom = Length::Auto();
  if (s.position != Position::kAbsolute) s.position = Position::kAbsolute;
  MarkDirty(world, w);
}

static void DragMove(World& world, wid w, Vec2 pos) {
  Movable* m = world.Get<Movable>(w);
  if (!m) return;
  f32 nx = m->origin_x + (pos.x - m->press.x);
  f32 ny = m->origin_y + (pos.y - m->press.y);
  Style& s = world.Get<StyleC>(w)->style;
  s.left_offset = Length::Px(nx);
  s.top = Length::Px(ny);
  MarkDirty(world, w);
  if (m->on_drag) m->on_drag(Vec2{nx, ny});
}

bool InputRouter::Process(wid root) {
  WidgetRegistry* reg = WidgetRegistry::Active();
  if (!root.valid() || !platform_ || !reg) return false;
  World& world = *reg;

  auto& queue = platform_->input_queue();
  bool consumed = false;

  // Stale handles (e.g. after a tree rebuild) resolve to null automatically;
  // no explicit pointer-validation pass is needed.

  // Process mouse movement -> hover detection
  if (queue.move_count > 0) {
    Vec2 pos = queue.move_events[queue.move_count - 1].position;  // latest
    mouse_pos_ = pos;
    wid new_hover = HitTest(world, root, pos);

    if (new_hover != hovered_) {
      if (hovered_.valid()) {
        SetHoverBit(world, hovered_, false);
        if (on_hover_) on_hover_(hovered_, false);
      }
      hovered_ = new_hover;
      if (new_hover.valid()) {
        SetHoverBit(world, new_hover, true);
        if (on_hover_) on_hover_(new_hover, true);
        platform_->SetCursor(ComputedStyle(world, new_hover).cursor);
      } else {
        platform_->SetCursor(Cursor::kAuto);
      }
      consumed = true;
    }

    // Drag detection: only enter drag mode if there's a draggable target
    // reachable from the pressed widget, otherwise leave drag_target_ null so
    // the release path still fires a click.
    if (pressed_.valid()) {
      Vec2 diff = {pos.x - drag_start_.x, pos.y - drag_start_.y};
      f32 dist2 = diff.x * diff.x + diff.y * diff.y;
      if (!dragging_ && dist2 > kDragThreshold * kDragThreshold) {
        // If the pressed widget (or an ancestor) has a DragHandle, climb to the
        // nearest Movable ancestor so "drag the header moves the panel".
        wid dt;
        wid w = pressed_;
        while (w.valid() && !world.Has<DragHandle>(w))
          w = world.Get<Hierarchy>(w)->parent;
        if (w.valid()) {
          wid anc = world.Get<Hierarchy>(w)->parent;
          while (anc.valid() && !world.Has<Movable>(anc))
            anc = world.Get<Hierarchy>(anc)->parent;
          if (anc.valid())
            dt = anc;
          else if (world.Has<Movable>(w))
            dt = w;
        } else if (world.Has<Movable>(pressed_)) {
          dt = pressed_;
        }
        if (dt.valid()) {
          dragging_ = true;
          drag_target_ = dt;
          DragStart(world, dt, drag_start_);
        }
      }
      if (dragging_ && drag_target_.valid()) {
        DragMove(world, drag_target_, pos);
        drag_prev_ = pos;
        consumed = true;
      }
    }
  }

  // Process mouse buttons -> press/click
  for (u32 i = 0; i < queue.button_count; ++i) {
    auto& evt = queue.button_events[i];
    wid target = HitTest(world, root, evt.position);

    if (evt.pressed) {
      pressed_ = target;
      drag_start_ = evt.position;
      drag_prev_ = evt.position;
      dragging_ = false;
      drag_target_ = kNullWidget;
      if (target.valid())
        SetWidgetState(world, target,
                       WidgetStateOf(world, target) | WidgetState::kPressed);
      set_focus(target);
      consumed = true;
    } else {
      // Release. Drag end is a no-op: the position was already committed by
      // DragMove during the drag.
      bool was_dragging = drag_target_.valid() && world.Alive(drag_target_);
      dragging_ = false;
      drag_target_ = kNullWidget;
      if (pressed_.valid()) {
        auto state = WidgetStateOf(world, pressed_);
        SetWidgetState(world, pressed_,
                       static_cast<WidgetState>(
                           static_cast<u16>(state) &
                           ~static_cast<u16>(WidgetState::kPressed)));

        // Click: press and release on same widget (only if not dragging)
        if (!was_dragging && pressed_ == target && target.valid()) {
          if (on_click_) on_click_(target, evt.button);
          ClickWidget(world, target);
        }
      }
      pressed_ = kNullWidget;
      consumed = true;
    }
  }

  // Process scroll -> bubble up to find a handler
  for (u32 i = 0; i < queue.scroll_count; ++i) {
    auto& evt = queue.scroll_events[i];
    wid w = HitTest(world, root, evt.position);
    while (w.valid()) {
      if (ScrollWidget(world, w, Vec2{-evt.delta.x * 40.0f, -evt.delta.y * 40.0f})) {
        consumed = true;
        break;
      }
      w = world.Get<Hierarchy>(w)->parent;
    }
  }

  // Tab navigation
  for (u32 i = 0; i < queue.key_count; ++i) {
    auto& evt = queue.key_events[i];
    if (evt.pressed && evt.key == 258 /* GLFW_KEY_TAB */) {
      bool reverse = (evt.mods & 0x0001 /* GLFW_MOD_SHIFT */) != 0;
      Vector<wid> focusable;
      Function<void(wid)> collect = [&](wid w) {
        if (world.Get<WidgetNode>(w)->tab_index >= 0) focusable.push_back(w);
        for (wid child : world.Get<Hierarchy>(w)->children) collect(child);
      };
      collect(root);
      if (focusable.empty()) continue;
      std::sort(focusable.begin(), focusable.end(), [&world](wid a, wid b) {
        return world.Get<WidgetNode>(a)->tab_index <
               world.Get<WidgetNode>(b)->tab_index;
      });
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
      if (handled) continue;

      // Enter or Space activates the focused widget (keyboard/gamepad nav),
      // unless it consumes text input (then Space is a literal character).
      if (focused_.valid() && !ConsumesTextInput(world, focused_) &&
          (evt.key == 257 /* GLFW_KEY_ENTER */ ||
           evt.key == 335 /* GLFW_KEY_KP_ENTER */ ||
           evt.key == 32 /* GLFW_KEY_SPACE */)) {
        if (on_click_) on_click_(focused_, MouseButton::kLeft);
        ClickWidget(world, focused_);
        consumed = true;
        continue;
      }
    }
    // Dispatch to focused widget (key-down / repeat only; no key-up handler).
    if (focused_.valid() && (evt.pressed || evt.repeat))
      consumed |= KeyDownWidget(world, focused_, evt.key, evt.mods);
  }

  // Dispatch character input to focused widget
  for (u32 i = 0; i < queue.char_count; ++i) {
    if (focused_.valid())
      consumed |= CharInputWidget(world, focused_, queue.char_events[i].codepoint);
  }

  // Process gamepad button events
  for (u32 i = 0; i < queue.gamepad_button_count; ++i) {
    auto& evt = queue.gamepad_button_events[i];
    if (!evt.pressed) continue;

    gamepad_nav_active_ = true;

    if (evt.button == GamepadButton::kA) {
      if (focused_.valid()) {
        if (on_click_) on_click_(focused_, MouseButton::kLeft);
        ClickWidget(world, focused_);
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

void InputRouter::RefreshHover(wid root) {
  WidgetRegistry* reg = WidgetRegistry::Active();
  if (!root.valid() || !platform_ || !reg) return;
  World& world = *reg;
  wid new_hover = HitTest(world, root, mouse_pos_);
  if (new_hover == hovered_) return;
  if (hovered_.valid()) {
    SetHoverBit(world, hovered_, false);
    if (on_hover_) on_hover_(hovered_, false);
  }
  hovered_ = new_hover;
  if (new_hover.valid()) {
    SetHoverBit(world, new_hover, true);
    if (on_hover_) on_hover_(new_hover, true);
    platform_->SetCursor(ComputedStyle(world, new_hover).cursor);
  } else {
    platform_->SetCursor(Cursor::kAuto);
  }
}

void InputRouter::set_hover(wid widget) {
  WidgetRegistry* reg = WidgetRegistry::Active();
  if (!reg) {
    hovered_ = widget;
    return;
  }
  World& world = *reg;
  if (hovered_ == widget) return;
  if (hovered_.valid()) {
    SetHoverBit(world, hovered_, false);
    if (on_hover_) on_hover_(hovered_, false);
  }
  hovered_ = widget;
  if (widget.valid()) {
    SetHoverBit(world, widget, true);
    if (on_hover_) on_hover_(widget, true);
    if (platform_) platform_->SetCursor(ComputedStyle(world, widget).cursor);
  } else if (platform_) {
    platform_->SetCursor(Cursor::kAuto);
  }
}

void InputRouter::set_focus(wid widget) {
  WidgetRegistry* reg = WidgetRegistry::Active();
  if (!reg) {
    focused_ = widget;
    return;
  }
  World& world = *reg;
  if (focused_ == widget) return;
  if (focused_.valid()) {
    auto state = WidgetStateOf(world, focused_);
    SetWidgetState(world, focused_,
                   static_cast<WidgetState>(
                       static_cast<u16>(state) &
                       ~static_cast<u16>(WidgetState::kFocused)));
  }
  focused_ = widget;
  if (widget.valid())
    SetWidgetState(world, widget,
                   WidgetStateOf(world, widget) | WidgetState::kFocused);
}

void InputRouter::ResetState() {
  if (WidgetRegistry* reg = WidgetRegistry::Active()) {
    World& world = *reg;
    if (hovered_.valid()) SetHoverBit(world, hovered_, false);
    if (pressed_.valid()) {
      auto state = WidgetStateOf(world, pressed_);
      SetWidgetState(world, pressed_,
                     static_cast<WidgetState>(
                         static_cast<u16>(state) &
                         ~static_cast<u16>(WidgetState::kPressed)));
    }
    if (focused_.valid()) {
      auto state = WidgetStateOf(world, focused_);
      SetWidgetState(world, focused_,
                     static_cast<WidgetState>(
                         static_cast<u16>(state) &
                         ~static_cast<u16>(WidgetState::kFocused)));
    }
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

void InputRouter::ProcessGamepadNavigation(wid root, f32 /*delta_time*/) {
  (void)root;
}

void InputRouter::NavigateFocus(wid root, i8 dir_x, i8 dir_y) {
  WidgetRegistry* reg = WidgetRegistry::Active();
  if (!root.valid() || !reg) return;
  World& world = *reg;

  Vector<wid> focusable;
  Function<void(wid)> collect = [&](wid w) {
    if (world.Get<WidgetNode>(w)->tab_index >= 0) focusable.push_back(w);
    for (wid child : world.Get<Hierarchy>(w)->children) collect(child);
  };
  collect(root);
  if (focusable.empty()) return;

  std::sort(focusable.begin(), focusable.end(), [&world](wid a, wid b) {
    return world.Get<WidgetNode>(a)->tab_index <
           world.Get<WidgetNode>(b)->tab_index;
  });

  if (!focused_.valid()) {
    set_focus(focusable.front());
    if (on_hover_) on_hover_(focusable.front(), true);
    return;
  }

  if (dir_y != 0) {
    auto it = std::find(focusable.begin(), focusable.end(), focused_);
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

  if (focused_.valid()) {
    if (on_hover_) on_hover_(focused_, true);
  }
}

}  // namespace ugui
