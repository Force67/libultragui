#ifndef ULTRAGUI_SCRIPTING_LUA_RUNTIME_H_
#define ULTRAGUI_SCRIPTING_LUA_RUNTIME_H_

#include <ultragui/core/types.h>

struct lua_State;

namespace ugui {

class Widget;

/// Embedded Lua scripting runtime for UI logic.
/// Provides bindings to the widget tree so .ugui event handlers can be
/// written in Lua.
class LuaRuntime {
public:
    bool Init();
    void Shutdown();

    /// Execute a Lua script string. Returns true on success.
    bool Exec(const char* script, const char* name = "chunk");

    /// Load and execute a Lua file.
    bool ExecFile(const char* path);

    /// Register a widget so Lua can access it via ugui.find("name").
    void RegisterWidget(Widget* widget);
    void UnregisterWidget(Widget* widget);
    void ClearWidgetRegistry();

    /// Call a named Lua function (used for on_click, etc.)
    bool CallHandler(const char* func_name, Widget* widget);

    /// Expose a C++ function to Lua under ugui.{name}
    using NativeFunction = Function<int(lua_State*)>;
    void RegisterFunction(const char* name, NativeFunction func);

    lua_State* state() const { return L_; }

    /// Access the widget registry (for Lua callbacks)
    Widget* FindRegisteredWidget(const char* name) const;

private:
    static int LuaUguiFind(lua_State* L);
    static int LuaUguiGetProp(lua_State* L);
    static int LuaUguiSetProp(lua_State* L);
    static int LuaUguiLog(lua_State* L);

    static LuaRuntime* FromState(lua_State* L);

    void RegisterApi();

    lua_State* L_ = nullptr;
    HashMap<String, Widget*> widget_registry_;
    Vector<NativeFunction*> native_functions_;
};

} // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_RUNTIME_H_
