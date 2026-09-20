// Tests for .ugui reusable components: parsing, prop substitution, slots,
// imports, and multi-class application. Headless (no GPU).

#include <ugui/idl/builder.h>
#include <ugui/idl/parser.h>
#include <ugui/layout/layout_tree.h>
#include <ugui/widgets/text.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

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

static ugui::UguiDocument Parse(const char* src) {
  ugui::UguiDocument doc;
  ugui::Vector<ugui::ParseError> errors;
  bool ok = ugui::ParseUgui(src, std::strlen(src), "test", doc, errors);
  for (auto& e : errors)
    std::printf("\n    parse error %s:%u: %s", e.file.c_str(), e.line,
                e.message.c_str());
  ASSERT(ok);
  return doc;
}

static ugui::wid BuildString(const char* src) {
  ugui::UguiDocument doc = Parse(src);
  ugui::UguiBuilder builder;
  return builder.Build(doc);
}

static ugui::wid FindByName(ugui::World& world, ugui::wid root,
                            const char* name) {
  if (auto* n = world.Get<ugui::WidgetNode>(root); n && n->name == name)
    return root;
  if (auto* h = world.Get<ugui::Hierarchy>(root)) {
    for (ugui::wid child : h->children)
      if (ugui::wid hit = FindByName(world, child, name); hit.valid())
        return hit;
  }
  return ugui::kNullWidget;
}

TEST(component_parses_props_and_root) {
  auto doc = Parse(R"(
    component stat_row {
      prop label: "HP";
      prop value: 0;
      panel row {
        layout: row;
        text lbl { text: $label; }
        text val { text: $value; }
      }
    }
  )");
  ASSERT(doc.components.size() == 1);
  const auto& c = doc.components[0];
  ASSERT(c.name == "stat_row");
  ASSERT(c.props.size() == 2);
  ASSERT(c.props.at("label") == "HP");
  ASSERT(c.props.at("value") == "0");
  ASSERT(c.root.type == "panel");
  ASSERT(c.root.children.size() == 2);
  ASSERT(c.root.children[0].properties.at("text") == "$label");
}

TEST(component_instance_substitutes_props) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    component stat_row {
      prop label: "HP";
      prop value: 0;
      panel row {
        text lbl { text: "$label: $value"; }
      }
    }
    panel main {
      stat_row mana { label: "Mana"; value: 42; }
    }
  )");
  ASSERT(root.valid());
  ugui::wid lbl = FindByName(world, root, "mana_lbl");
  ASSERT(lbl.valid());
  ASSERT(world.Get<ugui::TextContent>(lbl)->text == "Mana: 42");
  ugui::DestroyWidget(world, root);
}

TEST(component_uses_prop_defaults) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    component stat_row {
      prop label: "HP";
      panel row { text lbl { text: $label; } }
    }
    panel main { stat_row hp { } }
  )");
  ugui::wid lbl = FindByName(world, root, "hp_lbl");
  ASSERT(lbl.valid());
  ASSERT(world.Get<ugui::TextContent>(lbl)->text == "HP");
  ugui::DestroyWidget(world, root);
}

TEST(component_root_takes_instance_name) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    component card { panel body { padding: 8; } }
    panel main { card inventory { } }
  )");
  ASSERT(FindByName(world, root, "inventory").valid());
  ASSERT(!FindByName(world, root, "body").valid());
  ugui::DestroyWidget(world, root);
}

TEST(component_slot_receives_instance_children) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    component card {
      panel body {
        text heading { text: "head"; }
        slot { text fallback { text: "empty"; } }
      }
    }
    panel main {
      card filled { text extra { text: "custom"; } }
      card empty { }
    }
  )");
  ASSERT(FindByName(world, root, "extra").valid());
  ASSERT(FindByName(world, root, "empty_fallback").valid());
  // The filled instance must not contain its fallback content
  ugui::wid filled = FindByName(world, root, "filled");
  ASSERT(!FindByName(world, filled, "filled_fallback").valid());
  ugui::DestroyWidget(world, root);
}

TEST(component_instance_overrides_root_style) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    component card { panel body { width: 100; height: 50; } }
    panel main { card wide { width: 300; } }
  )");
  ugui::wid wide = FindByName(world, root, "wide");
  ASSERT(wide.valid());
  const ugui::Style& s = world.Get<ugui::StyleC>(wide)->style;
  ASSERT(s.width.value == 300.0f);
  ASSERT(s.height.value == 50.0f);
  ugui::DestroyWidget(world, root);
}

TEST(component_implicit_name_prop) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    component tab { panel body { text lbl { text: $name; } } }
    panel main { tab settings { } }
  )");
  ugui::wid lbl = FindByName(world, root, "settings_lbl");
  ASSERT(lbl.valid());
  ASSERT(world.Get<ugui::TextContent>(lbl)->text == "settings");
  ugui::DestroyWidget(world, root);
}

TEST(nested_components_expand) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    component icon { panel dot { width: 8; height: 8; } }
    component row {
      panel body { layout: row; icon marker { } }
    }
    panel main { row first { } }
  )");
  ASSERT(FindByName(world, root, "first_marker").valid());
  ugui::DestroyWidget(world, root);
}

TEST(recursive_component_does_not_hang) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    component loop { panel body { loop again { } } }
    panel main { loop start { } }
  )");
  // Expansion must terminate; whatever tree came out gets destroyed.
  if (root.valid()) ugui::DestroyWidget(world, root);
}

TEST(multiple_style_classes_apply_in_order) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    class base { width: 100; height: 40; }
    class wide { width: 250; }
    panel main {
      panel pill { class: base wide; }
    }
  )");
  ugui::wid pill = FindByName(world, root, "pill");
  ASSERT(pill.valid());
  const ugui::Style& s = world.Get<ugui::StyleC>(pill)->style;
  ASSERT(s.width.value == 250.0f);  // later class wins
  ASSERT(s.height.value == 40.0f);
  ugui::DestroyWidget(world, root);
}

TEST(flexbox_properties_parse) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    panel main {
      layout: row; flex-wrap: wrap; align-content: space-between;
      row-gap: 8; column-gap: 20;
      panel item { flex: 1; align-self: center; }
      panel fixed { flex-basis: 200; flex-shrink: 0; }
    }
  )");
  const ugui::Style& m =
      world.Get<ugui::StyleC>(FindByName(world, root, "main"))->style;
  ASSERT(m.flex_wrap == ugui::FlexWrap::kWrap);
  ASSERT(m.align_content == ugui::AlignContent::kSpaceBetween);
  ASSERT(m.row_gap == 8.0f);
  ASSERT(m.column_gap == 20.0f);

  const ugui::Style& item =
      world.Get<ugui::StyleC>(FindByName(world, root, "item"))->style;
  ASSERT(item.flex_grow == 1.0f);
  ASSERT(item.flex_basis.unit == ugui::Length::Unit::kPx);
  ASSERT(item.flex_basis.value == 0.0f);
  ASSERT(item.align_self == ugui::AlignSelf::kCenter);

  const ugui::Style& fixed =
      world.Get<ugui::StyleC>(FindByName(world, root, "fixed"))->style;
  ASSERT(fixed.flex_basis.value == 200.0f);
  ASSERT(fixed.flex_shrink == 0.0f);
  ugui::DestroyWidget(world, root);
}

TEST(flex_wrap_lays_out_multiple_lines) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::wid root = BuildString(R"(
    panel main {
      layout: row; flex-wrap: wrap; width: 250; height: 300; row-gap: 10;
      panel a { width: 100; height: 50; }
      panel b { width: 100; height: 50; }
      panel c { width: 100; height: 50; }
    }
  )");
  ugui::LayoutEngine engine;
  ugui::ComputeWidgetLayout(root, {800.0f, 600.0f, 1.0f}, engine);

  // 250px row fits two 100px items; the third wraps to a second line
  // offset by height + row-gap.
  const ugui::Rect& a = world.Get<ugui::Transform>(
      FindByName(world, root, "a"))->rect;
  const ugui::Rect& c = world.Get<ugui::Transform>(
      FindByName(world, root, "c"))->rect;
  ASSERT(a.y == c.y - 60.0f);
  ASSERT(c.x == a.x);
  ugui::DestroyWidget(world, root);
}

TEST(media_queries_reapply_on_viewport_change) {
  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::UguiDocument doc = Parse(R"(
    panel main {
      layout: row; width: 600;
      @media (max-width: 800) { layout: column; width: 300; }
    }
  )");
  ugui::UguiBuilder builder;
  builder.set_viewport_size({1280.0f, 720.0f});
  ugui::wid root = builder.Build(doc);
  ugui::wid main = FindByName(world, root, "main");
  ASSERT(main.valid());
  ASSERT(world.Get<ugui::StyleC>(main)->style.flex_direction ==
         ugui::FlexDirection::kRow);

  // Shrink below the breakpoint: override kicks in
  builder.set_viewport_size({640.0f, 720.0f});
  builder.ReapplyMediaQueries(root);
  ASSERT(world.Get<ugui::StyleC>(main)->style.flex_direction ==
         ugui::FlexDirection::kColumn);
  ASSERT(world.Get<ugui::StyleC>(main)->style.width.value == 300.0f);

  // Grow back: override falls away, base style returns
  builder.set_viewport_size({1280.0f, 720.0f});
  builder.ReapplyMediaQueries(root);
  ASSERT(world.Get<ugui::StyleC>(main)->style.flex_direction ==
         ugui::FlexDirection::kRow);
  ASSERT(world.Get<ugui::StyleC>(main)->style.width.value == 600.0f);

  ugui::DestroyWidget(world, root);
}

TEST(import_merges_components_and_classes) {
  const char* lib_path = "/tmp/ugui_test_import_lib.ugui";
  const char* main_path = "/tmp/ugui_test_import_main.ugui";
  {
    std::ofstream lib(lib_path);
    lib << R"(
      import "ugui_test_import_main.ugui";  // cycle: must not hang
      class accent { background: #ff0000; }
      component chip { panel body { text lbl { text: $name; } } }
    )";
    std::ofstream main(main_path);
    main << R"(
      import "ugui_test_import_lib.ugui";
      panel root { chip alpha { } }
    )";
  }

  ugui::UguiDocument doc;
  ugui::Vector<ugui::ParseError> errors;
  ASSERT(ugui::ParseUguiFile(main_path, doc, errors));
  ASSERT(doc.components.size() == 1);
  ASSERT(doc.style_classes.size() == 1);

  ugui::World& world = *ugui::WidgetRegistry::Active();
  ugui::UguiBuilder builder;
  ugui::wid root = builder.Build(doc);
  ASSERT(FindByName(world, root, "alpha_lbl").valid());
  ugui::DestroyWidget(world, root);

  std::remove(lib_path);
  std::remove(main_path);
}

int main() {
  std::printf("test_idl_components: %d/%d passed\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
