#ifndef ULTRAGUI_SCRIPTING_LUA_LOTTIE_H_
#define ULTRAGUI_SCRIPTING_LUA_LOTTIE_H_

#include <ugui/core/types.h>

namespace ugui {

class ScriptRuntime;
class LottieAnimation;
class Widget;

/// Register Lottie Lua bindings (ugui.load_lottie, ugui.lottie_play, etc.)
/// The loader function creates and returns a LottieAnimation*.
/// The find_widget function locates widgets by name for lottie_attach.
void RegisterLottieLua(
    ScriptRuntime& rt,
    Function<LottieAnimation*(const char*, unsigned, unsigned)> loader,
    Function<Widget*(const char*)> find_widget);

}  // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_LOTTIE_H_
