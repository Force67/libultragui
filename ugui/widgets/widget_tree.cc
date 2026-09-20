#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>
#include <ugui/widgets/widget_tree.h>

namespace ugui {

wid FindWidgetById(wid root, u32 id) {
  if (!root.valid()) return kNullWidget;
  WidgetRegistry& world = *WidgetRegistry::Active();
  if (world.Get<WidgetNode>(root)->id == id) return root;
  for (wid c : world.Get<Hierarchy>(root)->children) {
    wid found = FindWidgetById(c, id);
    if (found.valid()) return found;
  }
  return kNullWidget;
}

wid FindWidget(wid root, const char* name) {
  if (!root.valid()) return kNullWidget;
  WidgetRegistry& world = *WidgetRegistry::Active();
  if (world.Get<WidgetNode>(root)->name == name) return root;
  for (wid c : world.Get<Hierarchy>(root)->children) {
    wid found = FindWidget(c, name);
    if (found.valid()) return found;
  }
  return kNullWidget;
}

void UpdateWidgetTree(wid root, f64 dt) {
  if (!root.valid()) return;
  WidgetRegistry& world = *WidgetRegistry::Active();
  UpdateWidget(world, root, dt);
  // Nothing under a collapsed widget is laid out or painted, so there is no
  // per-frame state under it worth advancing: scroll momentum on a screen
  // nobody can see resumes when the screen comes back.
  if (world.Get<StyleC>(root)->style.visibility == Visibility::kCollapsed)
    return;
  for (wid child : world.Get<Hierarchy>(root)->children)
    UpdateWidgetTree(child, dt);
}

u32 MeasureWidgetTree(wid root) {
  if (!root.valid()) return 0;
  WidgetRegistry& world = *WidgetRegistry::Active();
  u32 measured = 1;
  // Nothing under a collapsed widget reaches layout or paint, so measuring it
  // (which means shaping its text) buys nothing. Uncollapsing marks the tree
  // dirty and measure runs before layout, so the frame it comes back is
  // already measured.
  if (world.Get<StyleC>(root)->style.visibility == Visibility::kCollapsed)
    return measured;
  for (wid child : world.Get<Hierarchy>(root)->children)
    measured += MeasureWidgetTree(child);
  f32 w = 0, h = 0;
  MeasureWidget(world, root, w, h);
  Transform* t = world.Get<Transform>(root);
  t->intrinsic_w = w;
  t->intrinsic_h = h;
  return measured;
}

}  // namespace ugui
