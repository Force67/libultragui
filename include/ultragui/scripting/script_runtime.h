#ifndef ULTRAGUI_SCRIPTING_SCRIPT_RUNTIME_H_
#define ULTRAGUI_SCRIPTING_SCRIPT_RUNTIME_H_

namespace ugui {

class Widget;

/// Abstract scripting runtime interface. Implement this to plug in any
/// scripting language (Lua, Python, Wren, ...) or use it as the C++ callback
/// dispatch point in engines that don't want a scripting layer at all.
///
/// The built-in LuaRuntime is the default implementation when ULTRAGUI_LUA
/// is enabled. When disabled, UIContext operates without a script runtime
/// and all scripting entry points become no-ops.
class ScriptRuntime {
public:
    virtual ~ScriptRuntime() = default;

    virtual bool Init() = 0;
    virtual void Shutdown() = 0;

    /// Execute a script from a string.
    virtual bool Exec(const char* script, const char* name = "chunk") = 0;

    /// Execute a script from a file path.
    virtual bool ExecFile(const char* path) = 0;

    /// Register a widget so the scripting layer can find it by name.
    virtual void RegisterWidget(Widget* widget) = 0;
    virtual void UnregisterWidget(Widget* widget) = 0;
    virtual void ClearWidgetRegistry() = 0;

    /// Call a named handler function, passing the widget as context.
    /// Returns true if the handler existed and ran successfully.
    virtual bool CallHandler(const char* func_name, Widget* widget) = 0;

    /// Look up a registered widget by name.
    virtual Widget* FindRegisteredWidget(const char* name) const = 0;
};

} // namespace ugui

#endif // ULTRAGUI_SCRIPTING_SCRIPT_RUNTIME_H_
