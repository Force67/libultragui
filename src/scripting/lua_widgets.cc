#include <ultragui/scripting/lua_widgets.h>
#include <ultragui/scripting/lua_runtime.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

void RegisterWidgetTreeLua(LuaRuntime& lua, Widget* root) {
    if (!root)
        return;
    if (!root->name().empty()) {
        lua.RegisterWidget(root);
    }
    for (u32 i = 0; i < root->child_count(); ++i) {
        RegisterWidgetTreeLua(lua, root->ChildAt(i));
    }
}

} // namespace ugui
