// LayoutEngine keeps its Yoga tree between Compute() calls so Yoga can reuse
// the layouts it has already solved. Everything here guards a way that can go
// wrong, and each of these failed at some point while the retention was being
// written:
//
//   - a style written only under a condition keeps the last frame's value
//     (`display: none` was the one that bit: a panel never came back),
//   - a measure function's answer changes without Yoga being able to see it,
//     so a node that re-shaped its text keeps its old size,
//   - the tree changes shape and node indices no longer name the same widgets.

#include <ugui/layout/layout_tree.h>
#include <ugui/widgets/components.h>
#include <ugui/widgets/panel.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

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

namespace {

constexpr ugui::LayoutViewport kViewport{400.0f, 400.0f, 1.0f};

ugui::Style& StyleOf(ugui::wid w) {
  return ugui::WidgetRegistry::Active()->Get<ugui::StyleC>(w)->style;
}

const ugui::Rect& RectOf(ugui::wid w) {
  return ugui::WidgetRegistry::Active()->Get<ugui::Transform>(w)->rect;
}

// A leaf's content size. Widgets with no measure vtable report whatever sits in
// their Transform, which is how a text widget's shaped size reaches layout.
void SetIntrinsic(ugui::wid w, float width, float height) {
  ugui::Transform* t = ugui::WidgetRegistry::Active()->Get<ugui::Transform>(w);
  t->intrinsic_w = width;
  t->intrinsic_h = height;
}

// A column that leaves its children at their content size, so what the test
// reads back is the measurement rather than a stretch.
ugui::wid MakeColumnRoot(ugui::u32 id) {
  ugui::wid root = ugui::WidgetRegistry::Active()->New(id);
  ugui::Style& s = StyleOf(root);
  s.flex_direction = ugui::FlexDirection::kColumn;
  s.align_items = ugui::AlignItems::kStart;
  s.width = ugui::Length::Px(400);
  s.height = ugui::Length::Px(400);
  return root;
}

bool Near(float a, float b) {
  return std::fabs(a - b) < 0.01f;
}

}  // namespace

// A node that re-measures has to be marked dirty by hand: Yoga caches what the
// measure function returned and cannot tell that the answer moved.
TEST(retained_tree_sees_intrinsic_size_change) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = MakeColumnRoot(1);
  ugui::wid leaf = world.New(2);
  ugui::AddChild(world, root, leaf);
  SetIntrinsic(leaf, 100.0f, 20.0f);

  ugui::LayoutEngine engine;
  ugui::Vector<ugui::LayoutNode> scratch;

  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(leaf).h, 20.0f));

  SetIntrinsic(leaf, 100.0f, 50.0f);
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(leaf).h, 50.0f));

  ugui::DestroyWidget(world, root);
}

// `display` is the property that showed why every style has to be written on
// every pass: it used to be set only when a widget was collapsed.
TEST(retained_tree_uncollapses) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = MakeColumnRoot(1);
  ugui::wid panel = world.New(2);
  ugui::AddChild(world, root, panel);
  StyleOf(panel).width = ugui::Length::Px(120);
  StyleOf(panel).height = ugui::Length::Px(60);

  ugui::LayoutEngine engine;
  ugui::Vector<ugui::LayoutNode> scratch;

  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).w, 120.0f));

  StyleOf(panel).visibility = ugui::Visibility::kCollapsed;
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).w, 0.0f));

  StyleOf(panel).visibility = ugui::Visibility::kVisible;
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).w, 120.0f));
  ASSERT(Near(RectOf(panel).h, 60.0f));

  ugui::DestroyWidget(world, root);
}

// The rest of the conditionally-written properties, in the state that used to
// stick: each is set, then taken back off again.
TEST(retained_tree_clears_optional_properties) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = MakeColumnRoot(1);
  ugui::wid panel = world.New(2);
  ugui::AddChild(world, root, panel);
  StyleOf(panel).width = ugui::Length::Px(100);
  StyleOf(panel).height = ugui::Length::Px(40);

  ugui::LayoutEngine engine;
  ugui::Vector<ugui::LayoutNode> scratch;

  // A min-width wider than the width wins, then stops applying.
  StyleOf(panel).min_width = ugui::Length::Px(200);
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).w, 200.0f));

  StyleOf(panel).min_width = ugui::Length::Px(0);
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).w, 100.0f));

  // A max-height clamps, then stops applying.
  StyleOf(panel).max_height = ugui::Length::Px(10);
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).h, 10.0f));

  StyleOf(panel).max_height = ugui::Length::Px(1e6f);
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).h, 40.0f));

  // An absolute offset moves the panel, then goes back to auto.
  StyleOf(panel).position = ugui::Position::kAbsolute;
  StyleOf(panel).left_offset = ugui::Length::Px(35);
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).x, 35.0f));

  StyleOf(panel).left_offset = ugui::Length::Auto();
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).x, 0.0f));

  ugui::DestroyWidget(world, root);
}

// Growing and shrinking the tree: the retained nodes are addressed by index,
// so a changed shape has to be noticed and rebuilt rather than reused.
TEST(retained_tree_rebuilds_on_shape_change) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = MakeColumnRoot(1);
  ugui::wid first = world.New(2);
  ugui::AddChild(world, root, first);
  StyleOf(first).width = ugui::Length::Px(50);
  StyleOf(first).height = ugui::Length::Px(30);

  ugui::LayoutEngine engine;
  ugui::Vector<ugui::LayoutNode> scratch;

  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(first).y, 0.0f));

  ugui::wid second = world.New(3);
  ugui::AddChild(world, root, second);
  StyleOf(second).width = ugui::Length::Px(50);
  StyleOf(second).height = ugui::Length::Px(30);

  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(first).y, 0.0f));
  ASSERT(Near(RectOf(second).y, 30.0f));  // stacked under the first
  ASSERT(Near(RectOf(second).w, 50.0f));

  ugui::DestroyWidget(world, second);
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(first).y, 0.0f));
  ASSERT(Near(RectOf(first).h, 30.0f));

  ugui::DestroyWidget(world, root);
}

// Two trees through one engine, which is what a document plus an overlay does.
// They must not evict each other's retained nodes.
TEST(retained_trees_do_not_evict_each_other) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid a_root = MakeColumnRoot(1);
  ugui::wid a_child = world.New(2);
  ugui::AddChild(world, a_root, a_child);
  StyleOf(a_child).width = ugui::Length::Px(80);
  StyleOf(a_child).height = ugui::Length::Px(25);

  ugui::wid b_root = MakeColumnRoot(3);
  ugui::wid b_child = world.New(4);
  ugui::AddChild(world, b_root, b_child);
  StyleOf(b_child).width = ugui::Length::Px(140);
  StyleOf(b_child).height = ugui::Length::Px(45);

  ugui::LayoutEngine engine;
  ugui::Vector<ugui::LayoutNode> scratch;

  for (int pass = 0; pass < 3; ++pass) {
    ugui::ComputeWidgetLayout(a_root, kViewport, engine, scratch);
    ugui::ComputeWidgetLayout(b_root, kViewport, engine, scratch);
    ASSERT(Near(RectOf(a_child).w, 80.0f));
    ASSERT(Near(RectOf(a_child).h, 25.0f));
    ASSERT(Near(RectOf(b_child).w, 140.0f));
    ASSERT(Near(RectOf(b_child).h, 45.0f));
  }

  ugui::DestroyWidget(world, a_root);
  ugui::DestroyWidget(world, b_root);
}

// Reset() drops the retained trees; the next pass has to build its own rather
// than read through a freed one.
TEST(retained_tree_survives_reset) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = MakeColumnRoot(1);
  ugui::wid panel = world.New(2);
  ugui::AddChild(world, root, panel);
  StyleOf(panel).width = ugui::Length::Px(70);
  StyleOf(panel).height = ugui::Length::Px(35);

  ugui::LayoutEngine engine;
  ugui::Vector<ugui::LayoutNode> scratch;

  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).w, 70.0f));

  engine.Reset();
  ugui::ComputeWidgetLayout(root, kViewport, engine, scratch);
  ASSERT(Near(RectOf(panel).w, 70.0f));
  ASSERT(Near(RectOf(panel).h, 35.0f));

  ugui::DestroyWidget(world, root);
}

int main() {
  std::printf("Retained layout tree test suite\n");
  std::printf("===============================\n");
  std::printf("\n%d/%d tests passed\n", tests_passed, tests_run);
  return (tests_passed == tests_run) ? 0 : 1;
}
