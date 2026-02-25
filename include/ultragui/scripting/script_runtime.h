#ifndef ULTRAGUI_SCRIPTING_SCRIPT_RUNTIME_H_
#define ULTRAGUI_SCRIPTING_SCRIPT_RUNTIME_H_

#if ULTRAGUI_LUA
#include <ultragui/core/types.h>
struct lua_State;
#endif

namespace ugui {

class Widget;

/// Concrete scripting runtime with link-time swappable implementation.
/// The default implementation uses Lua 5.4 (when ULTRAGUI_LUA=1);
/// a no-op stub is used when scripting is disabled.
/// Swap the .cc file via CMake (ULTRAGUI_SCRIPT_SOURCE) to provide
/// your own scripting backend.
class ScriptRuntime {
public:
    ScriptRuntime();
    ~ScriptRuntime();
    ScriptRuntime(const ScriptRuntime&) = delete;
    ScriptRuntime& operator=(const ScriptRuntime&) = delete;

    bool Init();
    void Shutdown();

    /// Execute a script from a string.
    bool Exec(const char* script, const char* name = "chunk");

    /// Execute a script from a file path.
    bool ExecFile(const char* path);

    /// Register a widget so the scripting layer can find it by name.
    void RegisterWidget(Widget* widget);
    void UnregisterWidget(Widget* widget);
    void ClearWidgetRegistry();

    /// Call a named handler function, passing the widget as context.
    /// Returns true if the handler existed and ran successfully.
    bool CallHandler(const char* func_name, Widget* widget);

    /// Look up a registered widget by name.
    Widget* FindRegisteredWidget(const char* name) const;

#if ULTRAGUI_LUA
    /// Expose a C++ function to Lua under ugui.{name}. Lua-specific.
    using NativeFunction = Function<int(lua_State*)>;
    void RegisterFunction(const char* name, NativeFunction func);

    lua_State* state() const;
#endif

    struct Impl;
private:
    Impl* impl_;
};

} // namespace ugui

#endif // ULTRAGUI_SCRIPTING_SCRIPT_RUNTIME_H_
