#pragma once

#include <functional>

namespace ugui {

class LuaRuntime;
class LottieAnimation;
class Widget;

/// Register Lottie Lua bindings (ugui.load_lottie, ugui.lottie_play, etc.)
/// The loader function creates and returns a LottieAnimation*.
/// The find_widget function locates widgets by name for lottie_attach.
void register_lottie_lua(
    LuaRuntime& lua,
    std::function<LottieAnimation*(const char*, unsigned, unsigned)> loader,
    std::function<Widget*(const char*)> find_widget);

} // namespace ugui
