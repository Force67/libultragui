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

// Collect the focus ring: every tab-indexed widget under root, in tab order.
// Hidden or collapsed subtrees are skipped whole, as painting skips them.
// Without an authored tab-index, a widget is focusable when it is an
// interactive kind (buttons, sliders, inputs, ...) or styled `cursor: pointer`.
static bool IsImplicitlyFocusable(WidgetRegistry& world, wid w, const Style& s) {
  switch (world.Get<WidgetNode>(w)->kind) {
    case WidgetKind::kButton:
    case WidgetKind::kCheckbox:
    case WidgetKind::kRadio:
    case WidgetKind::kToggle:
    case WidgetKind::kSlider:
    case WidgetKind::kDropdown:
    case WidgetKind::kTextInput:
      return true;
    default:
      break;
  }
  return s.cursor == Cursor::kPointer;
}

// Gather the focus ring under `w`, skipping anything hidden or collapsed.
// `implicit` widens the ring from authored tab-indices to anything that
// looks interactive; the caller uses it as a fallback.
static void CollectFocusable(WidgetRegistry& world, wid w, Vector<wid>& out,
                             bool implicit = false) {
  Style s = ComputedStyle(world, w);
  if (s.visibility == Visibility::kHidden ||
      s.visibility == Visibility::kCollapsed)
    return;
  const bool explicit_index = world.Get<WidgetNode>(w)->tab_index >= 0;
  if (explicit_index || (implicit && IsImplicitlyFocusable(world, w, s)))
    out.push_back(w);
  for (wid child : world.Get<Hierarchy>(w)->children)
    CollectFocusable(world, child, out, implicit);
}

// The ring to navigate: authored tab-indices, or when there are none, the
// implicitly focusable widgets. Decided per collection, not per document, so
// a host that concatenates every screen into one tree only asks the screen
// actually on display (collapsed screens are pruned above).
static void CollectFocusRing(WidgetRegistry& world, wid root, Vector<wid>& out,
                             bool allow_implicit) {
  CollectFocusable(world, root, out);
  if (out.empty() && allow_implicit)
    CollectFocusable(world, root, out, /*implicit=*/true);
}

// Order the ring by tab index. std::sort is unstable, so ties would shuffle
// between frames; the widget id breaks them to keep the walk repeatable.
static void SortFocusable(WidgetRegistry& world, Vector<wid>& ring) {
  std::sort(ring.begin(), ring.end(), [&world](wid a, wid b) {
    const i32 ta = world.Get<WidgetNode>(a)->tab_index;
    const i32 tb = world.Get<WidgetNode>(b)->tab_index;
    if (ta != tb) return ta < tb;
    return a.index < b.index;
  });
}

// Is the widget, or anything under its ancestors, hidden or collapsed?
// Focus survives a screen change, so a focused widget can be painted away;
// activating it would fire a click nobody aimed at.
static bool IsWidgetVisible(WidgetRegistry& world, wid w) {
  for (; w.valid(); w = world.Get<Hierarchy>(w)->parent) {
    Style s = ComputedStyle(world, w);
    if (s.visibility == Visibility::kHidden ||
        s.visibility == Visibility::kCollapsed)
      return false;
  }
  return true;
}

// Collect a widget and all of its ancestors up to the root.
static void CollectAncestorChain(World& world, wid w, std::vector<wid>& out) {
  while (w.valid()) {
    out.push_back(w);
    Hierarchy* h = world.Get<Hierarchy>(w);
    w = h ? h->parent : kNullWidget;
  }
}

// Move the :hover state between the old and new leaf's ancestor chains.
// HitTest returns the deepest widget only, but CSS hover semantics light up
// every ancestor too. Common ancestors are left untouched so a container's
// hover transition fires once on enter, not per internal child boundary.
static void UpdateHoverChain(World& world, wid old_leaf, wid new_leaf) {
  if (old_leaf == new_leaf) return;
  std::vector<wid> old_chain, new_chain;
  CollectAncestorChain(world, old_leaf, old_chain);
  CollectAncestorChain(world, new_leaf, new_chain);
  auto contains = [](const std::vector<wid>& v, wid x) {
    for (wid w : v)
      if (w == x) return true;
    return false;
  };
  for (wid w : old_chain)
    if (!contains(new_chain, w)) SetHoverBit(world, w, false);
  for (wid w : new_chain)
    if (!contains(old_chain, w)) SetHoverBit(world, w, true);
}

// The effective cursor for a hovered leaf is the nearest explicit cursor walking
// up the ancestor chain: a `cursor: pointer` row should still show the pointer
// when the mouse is over a child text/icon that didn't set its own cursor.
static Cursor ResolveCursor(World& world, wid leaf) {
  for (wid w = leaf; w.valid();) {
    Cursor c = ComputedStyle(world, w).cursor;
    if (c != Cursor::kAuto) return c;
    Hierarchy* h = world.Get<Hierarchy>(w);
    w = h ? h->parent : kNullWidget;
  }
  return Cursor::kAuto;
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
      if (hovered_.valid() && on_hover_) on_hover_(hovered_, false);
      UpdateHoverChain(world, hovered_, new_hover);
      hovered_ = new_hover;
      if (new_hover.valid()) {
        if (on_hover_) on_hover_(new_hover, true);
        platform_->SetCursor(ResolveCursor(world, new_hover));
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

  // Arrow-key navigation, the keyboard half of what the d-pad already does.
  // Handled before Tab so a screen can offer both; repeats count, so holding a
  // direction walks the ring at the platform's own key-repeat rate.
  if (keyboard_nav_) {
    for (u32 i = 0; i < queue.key_count; ++i) {
      auto& evt = queue.key_events[i];
      if (!evt.pressed) continue;
      i8 dir_x = 0, dir_y = 0;
      switch (evt.key) {
        case 262: dir_x = 1; break;   // GLFW_KEY_RIGHT
        case 263: dir_x = -1; break;  // GLFW_KEY_LEFT
        case 264: dir_y = 1; break;   // GLFW_KEY_DOWN
        case 265: dir_y = -1; break;  // GLFW_KEY_UP
        default: continue;
      }
      NavigateFocus(root, dir_x, dir_y);
      consumed = true;
    }
  }

  // Tab navigation
  for (u32 i = 0; i < queue.key_count; ++i) {
    auto& evt = queue.key_events[i];
    if (evt.pressed && evt.key == 258 /* GLFW_KEY_TAB */) {
      bool reverse = (evt.mods & 0x0001 /* GLFW_MOD_SHIFT */) != 0;
      Vector<wid> focusable;
      CollectFocusRing(world, root, focusable, keyboard_nav_);
      if (focusable.empty()) continue;
      SortFocusable(world, focusable);
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
      if (focused_.valid() && IsWidgetVisible(world, focused_) &&
          !ConsumesTextInput(world, focused_) &&
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
      if (focused_.valid() && IsWidgetVisible(world, focused_)) {
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
  if (hovered_.valid() && on_hover_) on_hover_(hovered_, false);
  UpdateHoverChain(world, hovered_, new_hover);
  hovered_ = new_hover;
  if (new_hover.valid()) {
    if (on_hover_) on_hover_(new_hover, true);
    platform_->SetCursor(ResolveCursor(world, new_hover));
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
  if (hovered_.valid() && on_hover_) on_hover_(hovered_, false);
  UpdateHoverChain(world, hovered_, widget);
  hovered_ = widget;
  if (widget.valid()) {
    if (on_hover_) on_hover_(widget, true);
    if (platform_) platform_->SetCursor(ResolveCursor(world, widget));
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
    if (hovered_.valid()) UpdateHoverChain(world, hovered_, kNullWidget);
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

// Centre of a widget's laid-out rect, which is what navigation reasons about:
// where a control looks like it is, not where it sits in the document.
static Vec2 FocusCentre(WidgetRegistry& world, wid w) {
  const Rect& r = world.Get<Transform>(w)->rect;
  return Vec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
}

// The widget to move to from `from` heading (dir_x, dir_y), or an invalid
// handle when there is nothing that way.
//
// Geometry, not document order: walking the tab ring for every direction
// makes Left/Up behave identically on grids. Candidates must lie in the
// direction asked for; nearest wins, with sideways drift penalized so a
// straight-ahead neighbour beats a closer diagonal one.
static wid NearestInDirection(WidgetRegistry& world, const Vector<wid>& ring,
                              wid from, i8 dir_x, i8 dir_y) {
  // Below this a candidate counts as level with the focus rather than beyond
  // it, which is what stops Left/Right walking a vertical list.
  constexpr f32 kMinTravel = 1.0f;
  constexpr f32 kDriftPenalty = 2.0f;

  const Vec2 origin = FocusCentre(world, from);
  wid best;
  f32 best_score = 0.0f;
  for (wid candidate : ring) {
    if (candidate == from) continue;
    const Vec2 to = FocusCentre(world, candidate);
    const f32 dx = to.x - origin.x, dy = to.y - origin.y;
    const f32 along = dx * dir_x + dy * dir_y;
    if (along < kMinTravel) continue;  // level with, or behind, the focus
    const f32 drift = std::abs(dx * -dir_y + dy * dir_x);
    const f32 score = along + drift * kDriftPenalty;
    if (!best.valid() || score < best_score) {
      best = candidate;
      best_score = score;
    }
  }
  return best;
}

// The far end of the ring in the OPPOSITE direction, so walking off the bottom
// of a list comes back on at the top. Same geometry test, so a vertical list
// still refuses to wrap horizontally: nothing is far enough sideways to qualify.
static wid WrapInDirection(WidgetRegistry& world, const Vector<wid>& ring,
                           wid from, i8 dir_x, i8 dir_y) {
  constexpr f32 kMinTravel = 1.0f;
  const Vec2 origin = FocusCentre(world, from);
  wid best;
  f32 best_distance = 0.0f;
  for (wid candidate : ring) {
    if (candidate == from) continue;
    const Vec2 to = FocusCentre(world, candidate);
    const f32 dx = to.x - origin.x, dy = to.y - origin.y;
    const f32 back = -(dx * dir_x + dy * dir_y);
    if (back < kMinTravel) continue;
    if (!best.valid() || back > best_distance) {
      best = candidate;
      best_distance = back;
    }
  }
  return best;
}

void InputRouter::NavigateFocus(wid root, i8 dir_x, i8 dir_y) {
  WidgetRegistry* reg = WidgetRegistry::Active();
  if (!root.valid() || !reg) return;
  World& world = *reg;

  Vector<wid> focusable;
  CollectFocusRing(world, root, focusable, keyboard_nav_);
  if (focusable.empty()) return;

  SortFocusable(world, focusable);

  // Nothing focused yet (a screen just opened, or the mouse has been driving):
  // the first press lands on the ring's head rather than moving from nowhere.
  if (!focused_.valid() ||
      std::find(focusable.begin(), focusable.end(), focused_) == focusable.end()) {
    set_focus(focusable.front());
    if (on_hover_) on_hover_(focusable.front(), true);
    return;
  }

  wid next = NearestInDirection(world, focusable, focused_, dir_x, dir_y);
  if (!next.valid())
    next = WrapInDirection(world, focusable, focused_, dir_x, dir_y);
  if (!next.valid()) return;  // nothing that way, and nothing to wrap to

  set_focus(next);
  if (on_hover_) on_hover_(next, true);
}

}  // namespace ugui
