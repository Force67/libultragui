#include <ugui/scripting/lua_widgets.h>
#include <ugui/scripting/script_runtime.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

namespace ugui {

void RegisterWidgetTree(ScriptRuntime& rt, wid root) {
  if (!root.valid()) return;
  World& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(root);
  if (!n) return;
  if (!n->name.empty()) {
    rt.RegisterWidget(root);
  }
  for (wid child : world.Get<Hierarchy>(root)->children) {
    RegisterWidgetTree(rt, child);
  }
}

}  // namespace ugui
