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

void ImageMeasure(WidgetRegistry& world, wid e, f32& out_w, f32& out_h) {
  ImageContent* c = world.Get<ImageContent>(e);
  out_w = c ? c->natural_w : 0.0f;
  out_h = c ? c->natural_h : 0.0f;
}

void ImageDraw(WidgetRegistry& world, wid e, Renderer2D& renderer) {
  ImageContent* c = world.Get<ImageContent>(e);
  if (!c || c->texture == kNullTextureId) return;
  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  renderer.DrawTexturedRect(world.Get<Transform>(e)->rect, c->texture,
                            Color::White().WithAlpha(s.opacity), image_radii(s));
}

}  // namespace

WidgetVTable ImageVTable() {
  WidgetVTable vt;
  vt.draw = ImageDraw;
  vt.measure = ImageMeasure;
  return vt;
}

wid CreateImage(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kImage;
  world.Add<ImageContent>(e, ImageContent{});
  return e;
}

void SetImageTexture(wid e, TextureId texture, f32 width, f32 height) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kImage) return;
  ImageContent& c = world.GetOrAdd<ImageContent>(e);
  c.texture = texture;
  c.natural_w = width;
  c.natural_h = height;
  MarkDirty(world, e);
}

}  // namespace ugui
