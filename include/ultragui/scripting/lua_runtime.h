#pragma once

#include <ultragui/core/types.h>

#include <functional>
#include <string>
#include <unordered_map>

struct lua_State;

namespace ugui {

class Widget;

/// Embedded Lua scripting runtime for UI logic.
/// Provides bindings to the widget tree so .ugui event handlers can be
/// written in Lua.
class LuaRuntime {
public:
    bool init();
    void shutdown();

    /// Execute a Lua script string. Returns true on success.
    bool exec(const char* script, const char* name = "chunk");

    /// Load and execute a Lua file.
    bool exec_file(const char* path);

    /// Register a widget so Lua can access it via ugui.find("name").
    void register_widget(Widget* widget);
    void unregister_widget(Widget* widget);

    /// Call a named Lua function (used for on_click, etc.)
    bool call_handler(const char* func_name, Widget* widget);

    /// Expose a C++ function to Lua under ugui.{name}
    using NativeFunction = std::function<int(lua_State*)>;
    void register_function(const char* name, NativeFunction func);

    lua_State* state() const { return L_; }

    /// Access the widget registry (for Lua callbacks)
    Widget* find_registered_widget(const char* name) const;

private:
    static int lua_ugui_find(lua_State* L);
    static int lua_ugui_get_prop(lua_State* L);
    static int lua_ugui_set_prop(lua_State* L);
    static int lua_ugui_log(lua_State* L);

    static LuaRuntime* from_state(lua_State* L);

    void register_api();

    lua_State* L_ = nullptr;
    std::unordered_map<std::string, Widget*> widget_registry_;
};

} // namespace ugui
