#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/widgets/image.h>
#include <ugui/widgets/widget_registry.h>

namespace ugui {
namespace {

u32 image_radii(const Style& s) {
  if (s.corner_radius_tl > 0.0f || s.corner_radius_tr > 0.0f ||
      s.corner_radius_br > 0.0f || s.corner_radius_bl > 0.0f) {
    return Vertex2D::PackRadii(s.corner_radius_tl, s.corner_radius_tr,
                               s.corner_radius_br, s.corner_radius_bl);
  }
  return Vertex2D::PackRadii(s.corner_radius);
}

void ImageMeasure(WidgetRegistry& world, Widget& w, f32& out_w, f32& out_h) {
  ImageContent* c = world.Get<ImageContent>(w.handle());
  out_w = c ? c->natural_w : 0.0f;
  out_h = c ? c->natural_h : 0.0f;
}

void ImageDraw(WidgetRegistry& world, Widget& w, Renderer2D& renderer) {
  ImageContent* c = world.Get<ImageContent>(w.handle());
  if (!c || c->texture == kInvalidTexture) return;
  Style s = w.ComputedStyle();
  s.Scale(w.ui_scale());
  renderer.DrawTexturedRect(w.rect(), c->texture,
                            Color::White().WithAlpha(s.opacity), image_radii(s));
}

}  // namespace

WidgetVTable ImageVTable() {
  WidgetVTable vt;
  vt.draw = ImageDraw;
  vt.measure = ImageMeasure;
  return vt;
}

Widget* CreateImage(u32 id) {
  Widget* w = new Widget(id);
  w->set_kind(WidgetKind::kImage);
  WidgetRegistry::Active()->Add<ImageContent>(w->handle(), ImageContent{});
  return w;
}

void SetImageTexture(Widget* w, RHITextureHandle texture, f32 width,
                     f32 height) {
  if (!w || w->kind() != WidgetKind::kImage || !w->registry()) return;
  ImageContent& c = w->registry()->GetOrAdd<ImageContent>(w->handle());
  c.texture = texture;
  c.natural_w = width;
  c.natural_h = height;
  w->MarkDirty();
}

}  // namespace ugui
