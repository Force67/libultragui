#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>
#include <ugui/widgets/widget_tree.h>

namespace ugui {

Widget* FindWidgetById(Widget* root, u32 id) {
  if (!root) return nullptr;
  if (root->id() == id) return root;
  for (auto* child : root->child_ptrs()) {
    if (auto* found = FindWidgetById(child, id)) return found;
  }
  return nullptr;
}

Widget* FindWidget(Widget* root, const char* name) {
  if (!root) return nullptr;
  if (root->name() == name) return root;
  for (auto* child : root->child_ptrs()) {
    if (auto* found = FindWidget(child, name)) return found;
  }
  return nullptr;
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
