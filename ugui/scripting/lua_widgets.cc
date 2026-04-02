#include <ugui/scripting/lua_widgets.h>
#include <ugui/scripting/script_runtime.h>
#include <ugui/widgets/widget.h>

namespace ugui {

void RegisterWidgetTree(ScriptRuntime& rt, Widget* root) {
  if (!root) return;
  if (!root->name().empty()) {
    rt.RegisterWidget(root);
  }
  for (Widget* child : root->child_ptrs()) {
    RegisterWidgetTree(rt, child);
  }
}

}  // namespace ugui
