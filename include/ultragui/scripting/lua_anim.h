#ifndef ULTRAGUI_SCRIPTING_LUA_ANIM_H_
#define ULTRAGUI_SCRIPTING_LUA_ANIM_H_

#include <functional>

namespace ugui {

class LuaRuntime;
class VectorAnimation;
class Widget;

/// Register .uganim Lua bindings (ugui.load_anim, ugui.anim_play, etc.)
void RegisterAnimLua(
    LuaRuntime& lua,
    std::function<VectorAnimation*(const char*, unsigned, unsigned)> loader,
    std::function<Widget*(const char*)> find_widget);

} // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_ANIM_H_
