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
  for (wid child : world.Get<Hierarchy>(root)->children)
    UpdateWidgetTree(child, dt);
}

void MeasureWidgetTree(wid root) {
  if (!root.valid()) return;
  WidgetRegistry& world = *WidgetRegistry::Active();
  for (wid child : world.Get<Hierarchy>(root)->children) MeasureWidgetTree(child);
  f32 w = 0, h = 0;
  MeasureWidget(world, root, w, h);
  Transform* t = world.Get<Transform>(root);
  t->intrinsic_w = w;
  t->intrinsic_h = h;
}

}  // namespace ugui
