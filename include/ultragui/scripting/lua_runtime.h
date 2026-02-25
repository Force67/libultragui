#ifndef ULTRAGUI_SCRIPTING_LUA_RUNTIME_H_
#define ULTRAGUI_SCRIPTING_LUA_RUNTIME_H_

#include <ultragui/core/types.h>
#include <ultragui/scripting/script_runtime.h>

struct lua_State;

namespace ugui {

/// Lua 5.4 implementation of ScriptRuntime. Default scripting backend.
/// Built when ULTRAGUI_LUA is enabled (the default).
class LuaRuntime : public ScriptRuntime {
public:
    bool Init() override;
    void Shutdown() override;
    bool Exec(const char* script, const char* name = "chunk") override;
    bool ExecFile(const char* path) override;
    void RegisterWidget(Widget* widget) override;
    void UnregisterWidget(Widget* widget) override;
    void ClearWidgetRegistry() override;
    bool CallHandler(const char* func_name, Widget* widget) override;
    Widget* FindRegisteredWidget(const char* name) const override;

    /// Expose a C++ function to Lua under ugui.{name}. Lua-specific.
    using NativeFunction = Function<int(lua_State*)>;
    void RegisterFunction(const char* name, NativeFunction func);

    lua_State* state() const { return L_; }

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
