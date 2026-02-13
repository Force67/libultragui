#pragma once

#include <functional>

namespace ugui {

class LuaRuntime;
class VectorAnimation;
class Widget;

/// Register .uganim Lua bindings (ugui.load_anim, ugui.anim_play, etc.)
void register_anim_lua(
    LuaRuntime& lua,
    std::function<VectorAnimation*(const char*, unsigned, unsigned)> loader,
    std::function<Widget*(const char*)> find_widget);

} // namespace ugui
