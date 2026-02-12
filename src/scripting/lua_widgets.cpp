#include <ultragui/scripting/lua_widgets.h>
#include <ultragui/scripting/lua_runtime.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

void register_widget_tree_lua(LuaRuntime& lua, Widget* root) {
    if (!root)
        return;
    if (!root->name().empty()) {
        lua.register_widget(root);
    }
    for (u32 i = 0; i < root->child_count(); ++i) {
        register_widget_tree_lua(lua, root->child_at(i));
    }
}

} // namespace ugui
