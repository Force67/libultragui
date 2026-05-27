#ifndef ULTRAGUI_SCRIPTING_LUA_WIDGETS_H_
#define ULTRAGUI_SCRIPTING_LUA_WIDGETS_H_

#include <ugui/core/handle.h>

namespace ugui {

class ScriptRuntime;

/// Register all named widgets in a tree with a scripting runtime.
void RegisterWidgetTree(ScriptRuntime& rt, wid root);

}  // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_WIDGETS_H_
