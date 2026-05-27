#ifndef ULTRAGUI_SCRIPTING_LUA_ANIM_H_
#define ULTRAGUI_SCRIPTING_LUA_ANIM_H_

#include <ugui/core/handle.h>
#include <ugui/core/types.h>

namespace ugui {

class ScriptRuntime;
class VectorAnimation;

/// Register .uganim Lua bindings (ugui.load_anim, ugui.anim_play, etc.)
void RegisterAnimLua(
    ScriptRuntime& rt,
    Function<VectorAnimation*(const char*, unsigned, unsigned)> loader,
    Function<wid(const char*)> find_widget);

}  // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_ANIM_H_
