#ifndef ULTRAGUI_SCRIPTING_LUA_WIDGETS_H_
#define ULTRAGUI_SCRIPTING_LUA_WIDGETS_H_

namespace ugui {

class ScriptRuntime;
class Widget;

/// Register all named widgets in a tree with a scripting runtime.
void RegisterWidgetTree(ScriptRuntime& rt, Widget* root);

}  // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_WIDGETS_H_
