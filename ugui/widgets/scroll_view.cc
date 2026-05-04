#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>
#include <cmath>

namespace ugui {
namespace {

bool ScrollViewScroll(WidgetRegistry& world, Widget& w, Vec2 delta) {
  ScrollViewContent* c = world.Get<ScrollViewContent>(w.handle());
  if (!c) return false;
  // Apply immediately for zero-latency feel, then kick velocity for momentum.
  c->offset += delta;
  c->velocity = delta * 6.0f;
  w.MarkDirty();
  return true;
}

void ScrollViewLayout(WidgetRegistry& world, Widget& w, const Rect& rect,
                      const Rect& content_rect) {
  ScrollViewContent* c = world.Get<ScrollViewContent>(w.handle());
  if (!c) return;

  // Total content size from children.
  c->content_size = Vec2{0, 0};
  for (const Widget* child : w.child_ptrs()) {
    Rect cr = child->rect();
    c->content_size.x = std::max(c->content_size.x, cr.x + cr.w - content_rect.x);
    c->content_size.y = std::max(c->content_size.y, cr.y + cr.h - content_rect.y);
  }

  f32 max_scroll_x = std::max(0.0f, c->content_size.x - content_rect.w);
  f32 max_scroll_y = std::max(0.0f, c->content_size.y - content_rect.h);
  c->offset.x = std::clamp(c->offset.x, 0.0f, max_scroll_x);
  c->offset.y = std::clamp(c->offset.y, 0.0f, max_scroll_y);
}

WidgetId ScrollViewHitTest(WidgetRegistry& world, Widget& w, Vec2 point) {
  if (!w.rect().contains(point)) return kNullWidget;
  ScrollViewContent* c = world.Get<ScrollViewContent>(w.handle());

  // Children are visually translated by the offset in the paint pass; translate
  // input coordinates back into the children's layout space.
  if (w.content_rect().contains(point)) {
    Vec2 child_point = point + (c ? c->offset : Vec2::Zero());
    const Vector<wid>& children = w.children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      if (Widget* child = world.Get(*it)) {
        wid hit = child->HitTest(child_point);
        if (hit.valid()) return hit;
      }
    }
  }
  return w.handle();
}

void ScrollViewUpdate(WidgetRegistry& world, Widget& w, f64 dt) {
  ScrollViewContent* c = world.Get<ScrollViewContent>(w.handle());
  if (!c) return;

  if (c->velocity.LengthSq() > 0.5f) {
    c->offset += c->velocity * static_cast<f32>(dt);

    // Decay normalized to 60fps so momentum feels identical at any framerate.
    f32 decay = std::pow(c->deceleration, static_cast<f32>(dt) * 60.0f);
    c->velocity *= decay;

    Rect content = w.content_rect();
    f32 max_scroll_x = std::max(0.0f, c->content_size.x - content.w);
    f32 max_scroll_y = std::max(0.0f, c->content_size.y - content.h);
    c->offset.x = std::clamp(c->offset.x, 0.0f, max_scroll_x);
    c->offset.y = std::clamp(c->offset.y, 0.0f, max_scroll_y);

    // Kill velocity at the edges to prevent jitter.
    if (c->offset.y <= 0.0f || c->offset.y >= max_scroll_y) c->velocity.y = 0.0f;
    if (c->offset.x <= 0.0f || c->offset.x >= max_scroll_x) c->velocity.x = 0.0f;

    w.MarkPaintDirty();
  } else {
    c->velocity = Vec2::Zero();
  }
}

void ScrollViewDraw(WidgetRegistry& world, Widget& w, Renderer2D& renderer) {
  // custom_paint: this widget owns its whole paint (background + scrollbar).
  ScrollViewContent* c = world.Get<ScrollViewContent>(w.handle());
  Style s = w.ComputedStyle();
  Rect rect = w.rect();
  Rect content = w.content_rect();

  if (s.background.a > 0.0f) {
    u32 radii =
        (s.corner_radius_tl > 0.0f || s.corner_radius_tr > 0.0f ||
         s.corner_radius_br > 0.0f || s.corner_radius_bl > 0.0f)
            ? Vertex2D::PackRadii(s.corner_radius_tl, s.corner_radius_tr,
                                  s.corner_radius_br, s.corner_radius_bl)
            : Vertex2D::PackRadii(s.corner_radius);
    renderer.DrawRect(rect, s.background.WithAlpha(s.background.a * s.opacity),
                      radii);
  }

  renderer.PushScissor(content);

  // Scrollbar indicator.
  Vec2 content_size = c ? c->content_size : Vec2::Zero();
  Vec2 offset = c ? c->offset : Vec2::Zero();
  if (content_size.y > content.h) {
    f32 visible_ratio = content.h / content_size.y;
    f32 bar_height = std::max(content.h * visible_ratio, 20.0f);
    f32 scroll_ratio = offset.y / (content_size.y - content.h);
    f32 bar_y = content.y + scroll_ratio * (content.h - bar_height);

    renderer.DrawRect({content.right() - 4, bar_y, 4, bar_height},
                      Color{1, 1, 1, 0.3f}, Vertex2D::PackRadii(2.0f));
  }

  renderer.PopScissor();
}

}  // namespace

WidgetVTable ScrollViewVTable() {
  WidgetVTable vt;
  vt.draw = ScrollViewDraw;
  vt.hit_test = ScrollViewHitTest;
  vt.on_scroll = ScrollViewScroll;
  vt.on_layout = ScrollViewLayout;
  vt.on_update = ScrollViewUpdate;
  vt.custom_paint = true;
  return vt;
}

Widget* CreateScrollView(u32 id) {
  Widget* w = new Widget(id);
  w->set_kind(WidgetKind::kScrollView);
  WidgetRegistry::Active()->Add<ScrollViewContent>(w->handle(),
                                                   ScrollViewContent{});
  return w;
}

Vec2 ScrollOffset(const Widget* w) {
  if (!w || w->kind() != WidgetKind::kScrollView || !w->registry())
    return Vec2::Zero();
  ScrollViewContent* c =
      w->registry()->Get<ScrollViewContent>(const_cast<Widget*>(w)->handle());
  return c ? c->offset : Vec2::Zero();
}

void SetScrollOffset(Widget* w, Vec2 offset) {
  if (!w || w->kind() != WidgetKind::kScrollView || !w->registry()) return;
  w->registry()->GetOrAdd<ScrollViewContent>(w->handle()).offset = offset;
  w->MarkDirty();
}

void ScrollBy(Widget* w, Vec2 delta) {
  if (!w || w->kind() != WidgetKind::kScrollView || !w->registry()) return;
  w->registry()->GetOrAdd<ScrollViewContent>(w->handle()).offset += delta;
  w->MarkDirty();
}

Vec2 ScrollContentSize(const Widget* w) {
  if (!w || w->kind() != WidgetKind::kScrollView || !w->registry())
    return Vec2::Zero();
  ScrollViewContent* c =
      w->registry()->Get<ScrollViewContent>(const_cast<Widget*>(w)->handle());
  return c ? c->content_size : Vec2::Zero();
}

}  // namespace ugui
