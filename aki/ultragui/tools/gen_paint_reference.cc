// Prints what the C++ layout + paint pass produce for a fixed panel tree
// (layout rects, then the DrawData of PaintWidgetTree in draw-data mode with
// a recording texture backend), for tests/paint_test.aki. Links the C++
// library (a Release build, so float contraction matches akiko's). From the
// repository root, in the dev shell:
//
//   cmake -S . -B /tmp/ugui-ref -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C /tmp/ugui-ref ultragui
//   clang++ -std=c++20 -O2 -I. -Ithird_party/yoga $(pkg-config --cflags freetype2 harfbuzz) \
//     aki/ultragui/tools/gen_paint_reference.cc /tmp/ugui-ref/libultragui.a /tmp/ugui-ref/third_party/yoga/yoga/libyogacore.a \
//     $(pkg-config --libs freetype2 harfbuzz) -llua -o /tmp/gen_paint_reference && /tmp/gen_paint_reference
//
// Line formats as gen_renderer_reference.cc, plus RECT id x y w h (bits).
// The scene must match tests/paint_test.aki's `build` step for step.
#include <ugui/layout/layout_tree.h>
#include <ugui/render/paint.h>
#include <ugui/render/renderer2d.h>
#include <ugui/widgets/panel.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

#include <stdio.h>
#include <string.h>

using namespace ugui;

static u32 Bits(f32 v) {
  u32 u;
  memcpy(&u, &v, 4);
  return u;
}

struct RecordingBackend : TextureBackend {
  u64 next = 100;
  TextureId CreateTexture(u32 w, u32 h, RHIFormat format, const void* pixels,
                          RHIFilter) override {
    u64 hash = 1469598103934665603ull;
    const u8* p = static_cast<const u8*>(pixels);
    for (u32 i = 0; i < w * h * 4; ++i) {
      hash ^= p[i];
      hash *= 1099511628211ull;
    }
    printf("TEX %u %u %d 0x%016llx\n", w, h, (int)format, (unsigned long long)hash);
    return next++;
  }
  void UpdateTexture(TextureId, const void*) override {}
  void DestroyTexture(TextureId) override {}
};

static void PrintVertex(const Vertex2D& v) {
  printf("V 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x "
         "0x%08x 0x%08x 0x%08x\n",
         Bits(v.pos[0]), Bits(v.pos[1]), Bits(v.uv[0]), Bits(v.uv[1]), v.color,
         v.color2, v.corner_radii, Bits(v.softness), Bits(v.half_size[0]),
         Bits(v.half_size[1]), Bits(v.border_width), v.border_color);
}

static Style& S(WidgetRegistry& w, wid e) { return w.Get<StyleC>(e)->style; }

int main() {
  WidgetRegistry world;
  WidgetRegistry::ScopedActive active(&world);
  WidgetContext ctx;
  ctx.ui_scale = 1.5f;

  wid root = CreatePanel(1);
  {
    Style& s = S(world, root);
    s.flex_direction = FlexDirection::kColumn;
    s.width = Length::Px(400);
    s.height = Length::Px(500);
    s.padding = EdgeInsets{10, 12, 14, 16};
    s.gap = 6.0f;
    s.background = Color{0.1f, 0.1f, 0.15f, 1.0f};
    s.corner_radius = 12.0f;
    s.overflow = Overflow::kHidden;
  }
  wid a = CreatePanel(2);
  AddChild(world, root, a);
  {
    Style& s = S(world, a);
    s.height = Length::Px(50);
    s.background = Color{0.9f, 0.2f, 0.1f, 1.0f};
    s.background_end = Color{0.1f, 0.2f, 0.9f, 1.0f};
    s.gradient_angle = 90.0f;
    s.border_width = 2.0f;
    s.border_color = Color{1, 1, 1, 0.8f};
    s.corner_radius = 7.0f;
    s.shadow = BoxShadow{Color{0, 0, 0, 0.5f}, 8.0f, 2.0f, Vec2{2.0f, 3.0f}, false};
  }
  wid b = CreatePanel(3);
  AddChild(world, root, b);
  {
    Style& s = S(world, b);
    s.flex_direction = FlexDirection::kRow;
    s.height = Length::Px(60);
    s.margin = EdgeInsets{0, 20, 0, 20};
    s.rotation = 15.0f;
    s.opacity = 0.5f;
    s.gradient_stop_count = 3;
    s.gradient_stops[0] = GradientStop{0.0f, Color{1, 0, 0, 1}};
    s.gradient_stops[1] = GradientStop{0.5f, Color{0, 1, 0, 1}};
    s.gradient_stops[2] = GradientStop{1.0f, Color{0, 0, 1, 1}};
    s.gradient_angle = 45.0f;
    s.border_width = 3.0f;
    s.border_color = Color{0.2f, 0.2f, 0.2f, 1.0f};
    s.corner_radius = 10.0f;
  }
  wid b1 = CreatePanel(4);
  AddChild(world, b, b1);
  {
    Style& s = S(world, b1);
    s.width = Length::Percent(40);
    s.flex_grow = 0.0f;
    s.background = Color{1, 1, 0, 1};
  }
  wid c = CreatePanel(5);
  AddChild(world, root, c);
  {
    Style& s = S(world, c);
    s.height = Length::Px(70);
    s.background = Color{0.3f, 0.6f, 0.3f, 1.0f};
    s.background_end = Color{0.0f, 0.0f, 0.0f, 0.0f};
    s.gradient_type = GradientType::kRadial;
    s.background_end = Color{0.0f, 0.1f, 0.0f, 0.5f};
    s.corner_radius_tl = 4.0f;
    s.corner_radius_tr = 16.0f;
    s.corner_radius_br = 2.0f;
    s.corner_radius_bl = 9.0f;
    s.shadow = BoxShadow{Color{0, 0, 0, 0.6f}, 5.0f, 1.0f, Vec2{1.0f, 1.0f}, true};
  }
  wid d = CreatePanel(6);
  AddChild(world, root, d);
  {
    Style& s = S(world, d);
    s.height = Length::Px(40);
    s.backdrop_blur = 10.0f;
    s.background = Color{1, 1, 1, 0.2f};
    s.border_width = 1.0f;
    s.border_color = Color{1, 1, 1, 0.05f};
    s.corner_radius = 6.0f;
  }
  world.Get<WidgetNode>(d)->tab_index = 0;
  wid e = CreatePanel(7);
  AddChild(world, root, e);
  {
    Style& s = S(world, e);
    s.height = Length::Px(30);
    s.background = Color{1, 0, 1, 1};
    s.visibility = Visibility::kHidden;
  }
  wid f = CreatePanel(8);
  AddChild(world, root, f);
  {
    Style& s = S(world, f);
    s.height = Length::Px(30);
    s.background = Color{0, 1, 1, 1};
    s.visibility = Visibility::kCollapsed;
  }
  wid g = CreatePanel(9);
  AddChild(world, root, g);
  {
    Style& s = S(world, g);
    s.flex_grow = 1.0f;
    s.gradient_stop_count = 2;
    s.gradient_stops[0] = GradientStop{0.0f, Color{0.2f, 0.2f, 0.2f, 1}};
    s.gradient_stops[1] = GradientStop{1.0f, Color{0.8f, 0.8f, 0.8f, 1}};
    s.gradient_type = GradientType::kRadial;
    s.corner_radius = 20.0f;
  }
  SetContext(world, root, &ctx);
  SetWidgetState(world, d, WidgetState::kFocused);

  LayoutEngine engine;
  LayoutViewport vp{800.0f, 600.0f, 1.5f};
  ComputeWidgetLayout(root, vp, engine);
  for (wid w : {root, a, b, b1, c, d, e, f, g}) {
    const Rect& r = world.Get<Transform>(w)->rect;
    const Rect& cr = world.Get<Transform>(w)->content_rect;
    printf("RECT %u 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
           world.Get<WidgetNode>(w)->id, Bits(r.x), Bits(r.y), Bits(r.w), Bits(r.h),
           Bits(cr.x), Bits(cr.y), Bits(cr.w), Bits(cr.h));
  }

  RecordingBackend backend;
  Renderer2D r;
  r.Init(nullptr);
  r.set_display_size({800.0f, 600.0f});
  r.set_texture_backend(&backend);
  r.BeginFrame();
  PaintWidgetTree(root, r);
  const DrawData& dd = r.GetDrawData();
  printf("COUNTS %u %u %u %u %u\n", dd.quad_vertex_count, dd.quad_index_count,
         dd.text_vertex_count, dd.text_index_count, dd.command_count);
  for (u32 i = 0; i < dd.quad_vertex_count; ++i) PrintVertex(dd.quad_vertices[i]);
  for (u32 i = 0; i < dd.text_vertex_count; ++i) PrintVertex(dd.text_vertices[i]);
  printf("IQ");
  for (u32 i = 0; i < dd.quad_index_count; ++i) printf(" %u", dd.quad_indices[i]);
  printf("\nIT");
  for (u32 i = 0; i < dd.text_index_count; ++i) printf(" %u", dd.text_indices[i]);
  printf("\n");
  for (u32 i = 0; i < dd.command_count; ++i) {
    const DrawCmd& cmd = dd.commands[i];
    printf("CMD 0x%08x 0x%08x 0x%08x 0x%08x %llu %u %u %d 0x%08x\n",
           Bits(cmd.clip_rect.x), Bits(cmd.clip_rect.y), Bits(cmd.clip_rect.w),
           Bits(cmd.clip_rect.h), (unsigned long long)cmd.texture_id, cmd.index_offset,
           cmd.elem_count, cmd.is_text ? 1 : 0, Bits(cmd.blur));
  }
  return 0;
}
