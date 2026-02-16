#ifndef ULTRAGUI_SCRIPTING_LUA_WIDGETS_H_
#define ULTRAGUI_SCRIPTING_LUA_WIDGETS_H_

namespace ugui {

class LuaRuntime;
class Widget;

/// Register all named widgets in a tree with the Lua runtime.
void RegisterWidgetTreeLua(LuaRuntime& lua, Widget* root);

} // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_WIDGETS_H_
