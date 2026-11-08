// Prints the widget-kind values tests/widget_kinds_test.aki compares
// against, computed by the C++ library. Needs a host-platform build of the
// library (no GLFW), then, inside the repo's dev shell:
//
//   cmake -S . -B /tmp/ugui-host -G Ninja -DCMAKE_BUILD_TYPE=Release \
//     -DULTRAGUI_PLATFORM_HOST=ON -DULTRAGUI_BUILD_EXAMPLES=OFF -DULTRAGUI_BUILD_TESTS=OFF
//   ninja -C /tmp/ugui-host ultragui
//   clang++ -std=c++20 -O2 -I. aki/ultragui/tools/gen_widget_reference.cc \
//     /tmp/ugui-host/libultragui.a $(pkg-config --libs freetype2 harfbuzz) \
//     -o /tmp/gen_widget_reference && /tmp/gen_widget_reference
//
// (add whatever further libraries the link asks for). Each scenario here has
// a twin in the test that builds the same widgets the same way.
#include <ugui/platform/platform.h>
#include <ugui/platform/platform_host.h>
#include <ugui/text/text_engine.h>
#include <ugui/widgets/button.h>
#include <ugui/widgets/checkbox.h>
#include <ugui/widgets/context_menu.h>
#include <ugui/widgets/dropdown.h>
#include <ugui/widgets/radio.h>
#include <ugui/widgets/rich_text.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/slider.h>
#include <ugui/widgets/text.h>
#include <ugui/widgets/text_input.h>
#include <ugui/widgets/toggle.h>
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

static void Size(const char* tag, WidgetRegistry& world, wid e) {
  f32 w = -1, h = -1;
  MeasureWidget(world, e, w, h);
  printf("M %s 0x%08x 0x%08x\n", tag, Bits(w), Bits(h));
}

static Style FontStyle16() {
  Style s;
  s.font_size = 16;
  return s;
}

// One pass of measures at a given ui scale.
static void Measures(WidgetRegistry& world, WidgetContext& ctx, const char* p) {
  char tag[64];
  auto T = [&](const char* name) {
    snprintf(tag, sizeof tag, "%s_%s", p, name);
    return tag;
  };

  wid t = CreateText(0);
  SetContext(world, t, &ctx);
  SetText(t, "Hello, World");
  SetStyle(world, t, FontStyle16());
  Size(T("text"), world, t);

  wid tu = CreateText(0);
  SetContext(world, tu, &ctx);
  SetText(tu, "hello wide world");
  Style su;
  su.font_size = 18;
  su.letter_spacing = 0.5f;
  su.text_transform = TextTransform::kUppercase;
  SetStyle(world, tu, su);
  Size(T("text_upper"), world, tu);
  su.text_transform = TextTransform::kCapitalize;
  SetStyle(world, tu, su);
  Size(T("text_cap"), world, tu);

  wid b = CreateButton(0);
  SetContext(world, b, &ctx);
  SetButtonLabel(b, "OK");
  Style bs;
  bs.font_size = 14;
  bs.padding = EdgeInsets(10, 20);
  SetStyle(world, b, bs);
  Size(T("button"), world, b);
  wid be = CreateButton(0);
  SetContext(world, be, &ctx);
  SetStyle(world, be, bs);
  Size(T("button_empty"), world, be);

  wid cb = CreateCheckbox(0);
  SetContext(world, cb, &ctx);
  SetCheckboxLabel(cb, "Enable sound");
  Size(T("checkbox"), world, cb);
  wid cbe = CreateCheckbox(0);
  SetContext(world, cbe, &ctx);
  Size(T("checkbox_empty"), world, cbe);

  wid r = CreateRadio(0);
  SetContext(world, r, &ctx);
  SetRadioLabel(r, "Option A");
  Style rs;
  rs.font_size = 15;
  SetStyle(world, r, rs);
  Size(T("radio"), world, r);

  wid tg = CreateToggle(0);
  SetContext(world, tg, &ctx);
  Style ps;
  ps.padding = EdgeInsets(3, 5);
  SetStyle(world, tg, ps);
  Size(T("toggle"), world, tg);

  wid sl = CreateSlider(0);
  SetContext(world, sl, &ctx);
  SetStyle(world, sl, ps);
  Size(T("slider"), world, sl);

  wid d = CreateDropdown(0);
  SetContext(world, d, &ctx);
  SetDropdownOptions(d, {"Low", "Medium", "Ultra high"});
  Style ds;
  ds.font_size = 16;
  ds.padding = EdgeInsets(4, 8);
  SetStyle(world, d, ds);
  Size(T("dropdown"), world, d);
  wid de = CreateDropdown(0);
  SetContext(world, de, &ctx);
  SetStyle(world, de, ds);
  Size(T("dropdown_empty"), world, de);

  wid ti = CreateTextInput(0);
  SetContext(world, ti, &ctx);
  SetTextInputValue(ti, "abc");
  Style is;
  is.font_size = 16;
  is.padding = EdgeInsets(4);
  SetStyle(world, ti, is);
  Size(T("text_input"), world, ti);
  wid tie = CreateTextInput(0);
  SetContext(world, tie, &ctx);
  SetStyle(world, tie, is);
  Size(T("text_input_empty"), world, tie);

  wid rt = CreateRichText(0);
  SetContext(world, rt, &ctx);
  TextSpan s1;
  s1.text = "Hello ";
  TextSpan s2;
  s2.text = "big";
  s2.font_size = 24;
  s2.font_weight = FontWeight::kBold;
  TextSpan s3;
  s3.text = " world";
  s3.color = Color::Red();
  AddRichTextSpan(rt, s1);
  AddRichTextSpan(rt, s2);
  AddRichTextSpan(rt, s3);
  SetStyle(world, rt, FontStyle16());
  Size(T("rich_text"), world, rt);

  wid cm = CreateContextMenu(0);
  SetContext(world, cm, &ctx);
  AddContextMenuItem(cm, "Copy", nullptr);
  AddContextMenuItem(cm, "Paste", nullptr);
  AddContextMenuSeparator(cm);
  AddContextMenuItem(cm, "Delete everything", nullptr);
  Style cs;
  cs.font_size = 13;
  cs.padding = EdgeInsets(6);
  SetStyle(world, cm, cs);
  Size(T("context_menu"), world, cm);
}

static void Feed(Platform& p, Vec2 mouse) {
  host::PushMouseMove(p, mouse);
  p.PollEvents();
}

int main() {
  TextEngine te;
  if (!te.Init(nullptr)) return 1;
  FontHandle f = te.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
  Platform platform;
  Platform::WindowConfig cfg;
  platform.Init(cfg);

  WidgetRegistry world;
  WidgetRegistry::ScopedActive active(&world);
  WidgetContext ctx;
  ctx.text_engine = &te;
  ctx.default_font = f;
  ctx.platform = &platform;

  ctx.ui_scale = 1.0f;
  Measures(world, ctx, "s1");
  WidgetContext ctx2 = ctx;
  ctx2.ui_scale = 1.25f;
  Measures(world, ctx2, "s125");

  // Slider: click maps the pointer onto the track, update follows it while
  // pressed.
  {
    wid sl = CreateSlider(0);
    SetContext(world, sl, &ctx);
    SetSliderMin(sl, -5.0f);
    SetSliderMax(sl, 17.5f);
    LayoutWidget(world, sl, {10, 5, 200, 24}, {10, 5, 200, 24});
    const f32 xs[] = {73.3f, 131.7f, 250.0f, 2.0f, 10.0f, 209.99f};
    Feed(platform, {xs[0], 10});
    ClickWidget(world, sl);
    printf("SLIDER 0x%08x\n", Bits(SliderValue(sl)));
    SetWidgetState(world, sl, WidgetState::kPressed);
    for (int i = 1; i < 6; ++i) {
      Feed(platform, {xs[i], 10});
      UpdateWidget(world, sl, 1.0 / 60.0);
      printf("SLIDER 0x%08x\n", Bits(SliderValue(sl)));
    }
  }

  // Toggle: thumb slide after a click.
  {
    wid tg = CreateToggle(0);
    SetContext(world, tg, &ctx);
    ClickWidget(world, tg);  // on; the click snaps the thumb to 1
    SetToggleOn(tg, false);  // snaps to 0
    SetWidgetState(world, tg, WidgetState::kChecked);  // on, thumb at 0
    for (int i = 0; i < 12; ++i) {
      UpdateWidget(world, tg, 1.0 / 60.0);
      printf("TOGGLE 0x%08x\n", Bits(world.Get<ToggleContent>(tg)->thumb_anim));
    }
    for (int i = 0; i < 40; ++i) {
      UpdateWidget(world, tg, 0.05);
    }
    printf("TOGGLE 0x%08x\n", Bits(world.Get<ToggleContent>(tg)->thumb_anim));
  }

  // Scroll view momentum.
  {
    wid sv = CreateScrollView(0);
    SetContext(world, sv, &ctx);
    wid child = CreateText(0);
    AddChild(world, sv, child);
    LayoutWidget(world, child, {0, 0, 100, 350}, {0, 0, 100, 350});
    LayoutWidget(world, sv, {0, 0, 100, 100}, {0, 0, 100, 100});
    Vec2 cs = ScrollContentSize(sv);
    printf("SCROLL_SIZE 0x%08x 0x%08x\n", Bits(cs.x), Bits(cs.y));
    ScrollWidget(world, sv, {0, 40});
    for (int i = 0; i < 8; ++i) {
      UpdateWidget(world, sv, 1.0 / 60.0);
      ScrollViewContent* c = world.Get<ScrollViewContent>(sv);
      printf("SCROLL 0x%08x 0x%08x\n", Bits(c->offset.y), Bits(c->velocity.y));
    }
    ScrollWidget(world, sv, {0, 1000});
    LayoutWidget(world, sv, {0, 0, 100, 100}, {0, 0, 100, 100});
    printf("SCROLL_CLAMP 0x%08x\n", Bits(ScrollOffset(world, sv).y));
  }

  // Text input: the caret under the pointer.
  {
    wid ti = CreateTextInput(0);
    SetContext(world, ti, &ctx);
    SetTextInputValue(ti, "Hello wide");
    SetStyle(world, ti, FontStyle16());
    LayoutWidget(world, ti, {20, 10, 200, 30}, {20, 10, 200, 30});
    const f32 xs[] = {0.0f, 24.0f, 31.0f, 47.5f, 80.0f, 300.0f};
    for (f32 x : xs) {
      Feed(platform, {x, 20});
      ClickWidget(world, ti);
      printf("CARET %u\n", world.Get<TextInputContent>(ti)->cursor);
    }
  }
  te.Shutdown();
  return 0;
}
