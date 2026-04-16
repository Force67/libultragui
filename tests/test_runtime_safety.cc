#include <ugui/scripting/script_runtime.h>
#include <ugui/widgets/components.h>
#include <ugui/widgets/image.h>
#include <ugui/widgets/panel.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/text.h>
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
  ugui::Panel root(1);
  ugui::ScrollView scroll(2);
  ugui::Panel child(3);

  root.AddChild(&scroll);
  scroll.AddChild(&child);

  root.OnLayout({0, 0, 400, 400}, {0, 0, 400, 400});
  scroll.OnLayout({0, 0, 200, 200}, {0, 0, 200, 200});
  child.OnLayout({10, 80, 50, 40}, {10, 80, 50, 40});
  scroll.set_scroll_offset({0, 50});

  ugui::wid hit = root.HitTest({20, 40});  // visual child y-range: 30..70
  ASSERT(hit == child.handle());

  // These widgets are stack-allocated; detach them so the parents' owning
  // destructors don't delete non-heap pointers as the stack unwinds.
  scroll.RemoveChild(&child);
  root.RemoveChild(&scroll);
}

TEST(widget_input_to_layout_point_accumulates_scroll_parents) {
  ugui::Panel root(1);
  ugui::ScrollView a(2);
  ugui::ScrollView b(3);
  ugui::Panel leaf(4);

  root.AddChild(&a);
  a.AddChild(&b);
  b.AddChild(&leaf);

  a.set_scroll_offset({5, 10});
  b.set_scroll_offset({0, 20});

  ugui::Vec2 p = leaf.InputToLayoutPoint({1, 2});
  ASSERT(p.x == 6.0f);
  ASSERT(p.y == 32.0f);

  // Detach stack-allocated widgets before the parents' destructors run.
  b.RemoveChild(&leaf);
  a.RemoveChild(&b);
  root.RemoveChild(&a);
}

TEST(script_runtime_widget_registry_can_be_cleared) {
  ugui::ScriptRuntime rt;
  ASSERT(rt.Init());

  ugui::Panel w(1);
  w.set_name("root");
  rt.RegisterWidget(&w);
  ASSERT(rt.FindRegisteredWidget("root") == &w);

  rt.ClearWidgetRegistry();
  ASSERT(rt.FindRegisteredWidget("root") == nullptr);

  rt.Shutdown();
}

namespace {
struct TestTag {
  int v;
};
}  // namespace

TEST(component_store_add_get_remove) {
  ugui::Panel w(1);
  ugui::World* world = ugui::WidgetRegistry::Active();

  world->Add<TestTag>(w.handle(), TestTag{42});
  ASSERT(world->Has<TestTag>(w.handle()));
  ASSERT(world->Get<TestTag>(w.handle())->v == 42);

  world->Add<TestTag>(w.handle(), TestTag{7});  // overwrite
  ASSERT(world->Get<TestTag>(w.handle())->v == 7);

  world->Remove<TestTag>(w.handle());
  ASSERT(!world->Has<TestTag>(w.handle()));
}

TEST(component_dropped_when_entity_released) {
  ugui::World* world = ugui::WidgetRegistry::Active();
  ugui::wid saved;
  {
    ugui::Panel w(2);
    saved = w.handle();
    world->Add<TestTag>(saved, TestTag{99});
    ASSERT(world->Has<TestTag>(saved));
  }
  // The widget is gone: its handle resolves to null and the component was
  // dropped during release (no leak, no stale read).
  ASSERT(world->Get(saved) == nullptr);
  ASSERT(!world->Has<TestTag>(saved));
}

TEST(tooltip_is_stored_as_component) {
  ugui::Panel w(3);
  ASSERT(w.tooltip().empty());
  w.set_tooltip("help");
  ASSERT(w.tooltip() == "help");
  ASSERT(ugui::WidgetRegistry::Active()->Has<ugui::Tooltip>(w.handle()));
}

TEST(state_styles_and_anim_live_in_components) {
  ugui::Panel w(4);
  ugui::World* world = ugui::WidgetRegistry::Active();
  ugui::Style override_style;

  // Plain widget carries neither component.
  ASSERT(!world->Has<ugui::StateStyle>(w.handle()));
  w.AddStateOverride(ugui::WidgetState::kHovered, override_style, 0);
  ASSERT(world->Has<ugui::StateStyle>(w.handle()));

  ASSERT(!world->Has<ugui::AnimStyle>(w.handle()));
  w.SetAnimationStyle(override_style);
  ASSERT(world->Has<ugui::AnimStyle>(w.handle()));
  w.ClearAnimationStyle();
  ASSERT(!world->Has<ugui::AnimStyle>(w.handle()));
}

TEST(image_is_a_generic_widget_with_component) {
  ugui::Widget* img = ugui::CreateImage(5);
  ASSERT(img->kind() == ugui::WidgetKind::kImage);
  ASSERT(img->registry()->Has<ugui::ImageContent>(img->handle()));

  ugui::SetImageTexture(img, 42, 64, 48);
  ugui::ImageContent* c = img->registry()->Get<ugui::ImageContent>(img->handle());
  ASSERT(c->texture == 42);
  ASSERT(c->natural_w == 64);

  // Measure dispatches through the vtable to read the component.
  float w = 0, h = 0;
  img->Measure(w, h);
  ASSERT(w == 64 && h == 48);

  delete img;
}

TEST(text_is_a_generic_widget_with_component) {
  ugui::Widget* t = ugui::CreateText(6);
  ASSERT(t->kind() == ugui::WidgetKind::kText);
  ASSERT(t->registry()->Has<ugui::TextContent>(t->handle()));

  ugui::SetText(t, "hello");
  ASSERT(t->registry()->Get<ugui::TextContent>(t->handle())->text == "hello");
  delete t;
}

int main() {
  std::printf("Runtime safety test suite\n");
  std::printf("=========================\n");
  std::printf("\n%d/%d tests passed\n", tests_passed, tests_run);
  return (tests_passed == tests_run) ? 0 : 1;
}
