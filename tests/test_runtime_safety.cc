#include <ugui/scripting/script_runtime.h>
#include <ugui/widgets/button.h>
#include <ugui/widgets/checkbox.h>
#include <ugui/widgets/components.h>
#include <ugui/widgets/image.h>
#include <ugui/widgets/panel.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/text.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

#include <cstdio>
#include <cstdlib>
#include <string>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                         \
  static void test_##name();               \
  static struct Register_##name {          \
    Register_##name() {                    \
      ++tests_run;                         \
      std::printf("  %-50s", #name "..."); \
      test_##name();                       \
      std::printf(" PASS\n");              \
      ++tests_passed;                      \
    }                                      \
  } reg_##name;                            \
  static void test_##name()

#define ASSERT(cond)                                                        \
  do {                                                                      \
    if (!(cond)) {                                                          \
      std::printf(" FAIL\n    Assertion failed: %s\n    at %s:%d\n", #cond, \
                  __FILE__, __LINE__);                                      \
      std::exit(1);                                                         \
    }                                                                       \
  } while (0)

TEST(scrollview_hit_test_respects_scroll_offset) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = world.New(1);
  ugui::wid scroll = ugui::CreateScrollView(2);
  ugui::wid child = world.New(3);

  ugui::AddChild(world, root, scroll);
  ugui::AddChild(world, scroll, child);

  ugui::LayoutWidget(world, root, {0, 0, 400, 400}, {0, 0, 400, 400});
  ugui::LayoutWidget(world, scroll, {0, 0, 200, 200}, {0, 0, 200, 200});
  ugui::LayoutWidget(world, child, {10, 80, 50, 40}, {10, 80, 50, 40});
  ugui::SetScrollOffset(scroll, {0, 50});

  ugui::wid hit = ugui::HitTest(world, root, {20, 40});  // visual y-range 30..70
  ASSERT(hit == child);

  ugui::DestroyWidget(world, root);
}

TEST(widget_input_to_layout_point_accumulates_scroll_parents) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = world.New(1);
  ugui::wid a = ugui::CreateScrollView(2);
  ugui::wid b = ugui::CreateScrollView(3);
  ugui::wid leaf = world.New(4);

  ugui::AddChild(world, root, a);
  ugui::AddChild(world, a, b);
  ugui::AddChild(world, b, leaf);

  ugui::SetScrollOffset(a, {5, 10});
  ugui::SetScrollOffset(b, {0, 20});

  ugui::Vec2 p = ugui::InputToLayoutPoint(world, leaf, {1, 2});
  ASSERT(p.x == 6.0f);
  ASSERT(p.y == 32.0f);

  ugui::DestroyWidget(world, root);
}

TEST(script_runtime_widget_registry_can_be_cleared) {
  ugui::ScriptRuntime rt;
  ASSERT(rt.Init());

  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid w = world.New(1);
  world.Get<ugui::WidgetNode>(w)->name = "root";
  rt.RegisterWidget(w);
  ASSERT(rt.FindRegisteredWidget("root") == w);

  rt.ClearWidgetRegistry();
  ASSERT(!rt.FindRegisteredWidget("root").valid());

  rt.Shutdown();
  ugui::DestroyWidget(world, w);
}

namespace {
struct TestTag {
  int v;
};
}  // namespace

TEST(component_store_add_get_remove) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid w = world.New(1);

  world.Add<TestTag>(w, TestTag{42});
  ASSERT(world.Has<TestTag>(w));
  ASSERT(world.Get<TestTag>(w)->v == 42);

  world.Add<TestTag>(w, TestTag{7});  // overwrite
  ASSERT(world.Get<TestTag>(w)->v == 7);

  world.Remove<TestTag>(w);
  ASSERT(!world.Has<TestTag>(w));

  ugui::DestroyWidget(world, w);
}

TEST(component_dropped_when_entity_released) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid w = world.New(2);
  ugui::wid saved = w;
  world.Add<TestTag>(saved, TestTag{99});
  ASSERT(world.Has<TestTag>(saved));

  ugui::DestroyWidget(world, w);

  // The widget is gone: its handle resolves to dead and the component was
  // dropped during release (no leak, no stale read).
  ASSERT(!world.Alive(saved));
  ASSERT(!world.Has<TestTag>(saved));
}

TEST(tooltip_is_stored_as_component) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid w = world.New(3);
  ASSERT(ugui::TooltipText(world, w).empty());
  ugui::SetTooltip(world, w, "help");
  ASSERT(ugui::TooltipText(world, w) == "help");
  ASSERT(world.Has<ugui::Tooltip>(w));
  ugui::DestroyWidget(world, w);
}

TEST(state_styles_and_anim_live_in_components) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid w = world.New(4);
  ugui::Style override_style;

  // Plain widget carries neither component.
  ASSERT(!world.Has<ugui::StateStyle>(w));
  ugui::AddStateOverride(world, w, ugui::WidgetState::kHovered, override_style, 0);
  ASSERT(world.Has<ugui::StateStyle>(w));

  ASSERT(!world.Has<ugui::AnimStyle>(w));
  ugui::SetAnimationStyle(world, w, override_style);
  ASSERT(world.Has<ugui::AnimStyle>(w));
  ugui::ClearAnimationStyle(world, w);
  ASSERT(!world.Has<ugui::AnimStyle>(w));

  ugui::DestroyWidget(world, w);
}

TEST(image_is_a_generic_widget_with_component) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid img = ugui::CreateImage(5);
  ASSERT(world.Get<ugui::WidgetNode>(img)->kind == ugui::WidgetKind::kImage);
  ASSERT(world.Has<ugui::ImageContent>(img));

  ugui::SetImageTexture(img, 42, 64, 48);
  ugui::ImageContent* c = world.Get<ugui::ImageContent>(img);
  ASSERT(c->texture == 42);
  ASSERT(c->natural_w == 64);

  // Measure dispatches through the vtable to read the component.
  float w = 0, h = 0;
  ugui::MeasureWidget(world, img, w, h);
  ASSERT(w == 64 && h == 48);

  ugui::DestroyWidget(world, img);
}

TEST(text_is_a_generic_widget_with_component) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid t = ugui::CreateText(6);
  ASSERT(world.Get<ugui::WidgetNode>(t)->kind == ugui::WidgetKind::kText);
  ASSERT(world.Has<ugui::TextContent>(t));

  ugui::SetText(t, "hello");
  ASSERT(world.Get<ugui::TextContent>(t)->text == "hello");
  ugui::DestroyWidget(world, t);
}

TEST(button_click_dispatches_through_vtable) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid b = ugui::CreateButton(7);
  ASSERT(world.Get<ugui::WidgetNode>(b)->kind == ugui::WidgetKind::kButton);
  ugui::SetButtonLabel(b, "OK");
  ASSERT(world.Get<ugui::ButtonContent>(b)->label == "OK");

  int clicks = 0;
  ugui::SetButtonClick(b, [&clicks]() { ++clicks; });
  ASSERT(ugui::ClickWidget(world, b));  // dispatch -> vtable.on_click -> handler
  ASSERT(clicks == 1);
  ugui::DestroyWidget(world, b);
}

TEST(checkbox_toggles_and_fires_change_through_vtable) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid cb = ugui::CreateCheckbox(8);
  ASSERT(world.Get<ugui::WidgetNode>(cb)->kind == ugui::WidgetKind::kCheckbox);
  ASSERT(!ugui::IsChecked(cb));

  int changes = 0;
  bool last = false;
  ugui::SetCheckboxChange(cb, [&](bool v) {
    ++changes;
    last = v;
  });
  ASSERT(ugui::ClickWidget(world, cb));  // toggle on -> fires on_change
  ASSERT(ugui::IsChecked(cb));
  ASSERT(changes == 1 && last == true);

  ugui::SetChecked(cb, false);  // programmatic: no on_change
  ASSERT(!ugui::IsChecked(cb));
  ASSERT(changes == 1);
  ugui::DestroyWidget(world, cb);
}

int main() {
  std::printf("Runtime safety test suite\n");
  std::printf("=========================\n");
  std::printf("\n%d/%d tests passed\n", tests_passed, tests_run);
  return (tests_passed == tests_run) ? 0 : 1;
}
