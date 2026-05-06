#include <ugui/scripting/script_runtime.h>
#include <ugui/widgets/button.h>
#include <ugui/widgets/checkbox.h>
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
  ugui::Widget root(1);
  ugui::Widget* scroll = ugui::CreateScrollView(2);
  ugui::Widget child(3);

  root.AddChild(scroll);
  scroll->AddChild(&child);

  root.OnLayout({0, 0, 400, 400}, {0, 0, 400, 400});
  scroll->OnLayout({0, 0, 200, 200}, {0, 0, 200, 200});
  child.OnLayout({10, 80, 50, 40}, {10, 80, 50, 40});
  ugui::SetScrollOffset(scroll, {0, 50});

  ugui::wid hit = root.HitTest({20, 40});  // visual child y-range: 30..70
  ASSERT(hit == child.handle());

  // child is stack-allocated; detach it so scroll's destructor does not delete a
  // non-heap pointer, then delete the heap scroll view.
  scroll->RemoveChild(&child);
  root.RemoveChild(scroll);
  delete scroll;
}

TEST(widget_input_to_layout_point_accumulates_scroll_parents) {
  ugui::Widget root(1);
  ugui::Widget* a = ugui::CreateScrollView(2);
  ugui::Widget* b = ugui::CreateScrollView(3);
  ugui::Widget leaf(4);

  root.AddChild(a);
  a->AddChild(b);
  b->AddChild(&leaf);

  ugui::SetScrollOffset(a, {5, 10});
  ugui::SetScrollOffset(b, {0, 20});

  ugui::Vec2 p = leaf.InputToLayoutPoint({1, 2});
  ASSERT(p.x == 6.0f);
  ASSERT(p.y == 32.0f);

  // Detach the stack-allocated leaf, then delete the heap scroll views (delete a
  // recursively deletes its child b).
  b->RemoveChild(&leaf);
  root.RemoveChild(a);
  delete a;
}

TEST(script_runtime_widget_registry_can_be_cleared) {
  ugui::ScriptRuntime rt;
  ASSERT(rt.Init());

  ugui::Widget w(1);
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
  ugui::Widget w(1);
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
    ugui::Widget w(2);
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
  ugui::Widget w(3);
  ASSERT(w.tooltip().empty());
  w.set_tooltip("help");
  ASSERT(w.tooltip() == "help");
  ASSERT(ugui::WidgetRegistry::Active()->Has<ugui::Tooltip>(w.handle()));
}

TEST(state_styles_and_anim_live_in_components) {
  ugui::Widget w(4);
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

TEST(button_click_dispatches_through_vtable) {
  ugui::Widget* b = ugui::CreateButton(7);
  ASSERT(b->kind() == ugui::WidgetKind::kButton);
  ugui::SetButtonLabel(b, "OK");
  ASSERT(b->registry()->Get<ugui::ButtonContent>(b->handle())->label == "OK");

  int clicks = 0;
  ugui::SetButtonClick(b, [&clicks]() { ++clicks; });
  ASSERT(b->OnClick());  // base OnClick -> vtable.on_click -> component handler
  ASSERT(clicks == 1);
  delete b;
}

TEST(checkbox_toggles_and_fires_change_through_vtable) {
  ugui::Widget* cb = ugui::CreateCheckbox(8);
  ASSERT(cb->kind() == ugui::WidgetKind::kCheckbox);
  ASSERT(!ugui::IsChecked(cb));

  int changes = 0;
  bool last = false;
  ugui::SetCheckboxChange(cb, [&](bool v) {
    ++changes;
    last = v;
  });
  ASSERT(cb->OnClick());  // toggle on -> fires on_change
  ASSERT(ugui::IsChecked(cb));
  ASSERT(changes == 1 && last == true);

  ugui::SetChecked(cb, false);  // programmatic: no on_change
  ASSERT(!ugui::IsChecked(cb));
  ASSERT(changes == 1);
  delete cb;
}

int main() {
  std::printf("Runtime safety test suite\n");
  std::printf("=========================\n");
  std::printf("\n%d/%d tests passed\n", tests_passed, tests_run);
  return (tests_passed == tests_run) ? 0 : 1;
}
