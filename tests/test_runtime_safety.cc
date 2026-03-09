#include <ultragui/scripting/script_runtime.h>
#include <ultragui/widgets/panel.h>
#include <ultragui/widgets/scroll_view.h>

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

  ugui::Widget* hit = root.HitTest({20, 40});  // visual child y-range: 30..70
  ASSERT(hit == &child);
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

int main() {
  std::printf("Runtime safety test suite\n");
  std::printf("=========================\n");
  std::printf("\n%d/%d tests passed\n", tests_passed, tests_run);
  return (tests_passed == tests_run) ? 0 : 1;
}
