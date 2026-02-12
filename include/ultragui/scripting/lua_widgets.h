#pragma once

namespace ugui {

class LuaRuntime;
class Widget;

/// Register all named widgets in a tree with the Lua runtime.
void register_widget_tree_lua(LuaRuntime& lua, Widget* root);

} // namespace ugui
