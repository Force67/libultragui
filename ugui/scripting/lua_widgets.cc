#include <ugui/scripting/lua_widgets.h>
#include <ugui/scripting/script_runtime.h>
#include <ugui/widgets/widget.h>

namespace ugui {

void RegisterWidgetTree(ScriptRuntime& rt, Widget* root) {
  if (!root) return;
  if (!root->name().empty()) {
    rt.RegisterWidget(root);
  }
  for (u32 i = 0; i < root->child_count(); ++i) {
    RegisterWidgetTree(rt, root->ChildAt(i));
  }
}

}  // namespace ugui
