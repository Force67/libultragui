#include <ultragui/scripting/script_runtime.h>
#include <ultragui/widgets/button.h>
#include <ultragui/widgets/text.h>
#include <ultragui/widgets/widget.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <charconv>
#include <cstdio>
#include <cstring>

namespace ugui {

// Registry key for storing the Impl* in Lua's registry
static const char* const REGISTRY_KEY = "ugui_runtime";

struct ScriptRuntime::Impl {
    lua_State* L = nullptr;
    HashMap<String, Widget*> widget_registry;
    Vector<ScriptRuntime::NativeFunction*> native_functions;

    static Impl* FromState(lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
        auto* rt = static_cast<Impl*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        return rt;
    }

    void RegisterApi();
    static int LuaUguiFind(lua_State* L);
    static int LuaUguiGetProp(lua_State* L);
    static int LuaUguiSetProp(lua_State* L);
    static int LuaUguiLog(lua_State* L);
};

ScriptRuntime::ScriptRuntime() : impl_(new Impl()) {}
ScriptRuntime::~ScriptRuntime() { delete impl_; }

bool ScriptRuntime::Init() {
    impl_->L = luaL_newstate();
    if (!impl_->L)
        return false;

    // Open safe standard libraries (no io/os for sandboxing)
    luaL_requiref(impl_->L, "_G", luaopen_base, 1);
    lua_pop(impl_->L, 1);
    luaL_requiref(impl_->L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(impl_->L, 1);
    luaL_requiref(impl_->L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(impl_->L, 1);
    luaL_requiref(impl_->L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(impl_->L, 1);

    // Store impl in Lua's registry so static callbacks can find us
    lua_pushlightuserdata(impl_->L, impl_);
    lua_setfield(impl_->L, LUA_REGISTRYINDEX, REGISTRY_KEY);

    impl_->RegisterApi();
    return true;
}

void ScriptRuntime::Shutdown() {
    if (impl_->L) {
        lua_close(impl_->L);
        impl_->L = nullptr;
    }
    for (auto* fn : impl_->native_functions) {
        delete fn;
    }
    impl_->native_functions.clear();
    impl_->widget_registry.clear();
}

bool ScriptRuntime::Exec(const char* script, const char* name) {
    if (luaL_loadbuffer(impl_->L, script, strlen(script), name) != LUA_OK) {
        std::fprintf(stderr, "ultragui/lua: load error: %s\n", lua_tostring(impl_->L, -1));
        lua_pop(impl_->L, 1);
        return false;
    }
    if (lua_pcall(impl_->L, 0, 0, 0) != LUA_OK) {
        std::fprintf(stderr, "ultragui/lua: runtime error: %s\n", lua_tostring(impl_->L, -1));
        lua_pop(impl_->L, 1);
        return false;
    }
    return true;
}

bool ScriptRuntime::ExecFile(const char* path) {
    if (luaL_loadfile(impl_->L, path) != LUA_OK) {
        std::fprintf(stderr, "ultragui/lua: failed to load '%s': %s\n", path, lua_tostring(impl_->L, -1));
        lua_pop(impl_->L, 1);
        return false;
    }
    if (lua_pcall(impl_->L, 0, 0, 0) != LUA_OK) {
        std::fprintf(stderr, "ultragui/lua: error in '%s': %s\n", path, lua_tostring(impl_->L, -1));
        lua_pop(impl_->L, 1);
        return false;
    }
    return true;
}

void ScriptRuntime::RegisterWidget(Widget* widget) {
    if (!widget->name().empty()) {
        impl_->widget_registry[widget->name()] = widget;
    }
}

void ScriptRuntime::UnregisterWidget(Widget* widget) {
    if (!widget->name().empty()) {
        impl_->widget_registry.erase(widget->name());
    }
}

void ScriptRuntime::ClearWidgetRegistry() {
    impl_->widget_registry.clear();
}

Widget* ScriptRuntime::FindRegisteredWidget(const char* name) const {
    auto it = impl_->widget_registry.find(name);
    return it != impl_->widget_registry.end() ? it->second : nullptr;
}

bool ScriptRuntime::CallHandler(const char* func_name, Widget* widget) {
    lua_getglobal(impl_->L, func_name);
    if (!lua_isfunction(impl_->L, -1)) {
        lua_pop(impl_->L, 1);
        return false;
    }

    // Push widget as a table with name and id
    lua_newtable(impl_->L);
    lua_pushstring(impl_->L, widget->name().c_str());
    lua_setfield(impl_->L, -2, "name");
    lua_pushinteger(impl_->L, widget->id());
    lua_setfield(impl_->L, -2, "id");

    if (lua_pcall(impl_->L, 1, 0, 0) != LUA_OK) {
        std::fprintf(stderr, "ultragui/lua: error calling '%s': %s\n", func_name,
                     lua_tostring(impl_->L, -1));
        lua_pop(impl_->L, 1);
        return false;
    }
    return true;
}

void ScriptRuntime::RegisterFunction(const char* name, NativeFunction func) {
    lua_getglobal(impl_->L, "ugui");
    if (!lua_istable(impl_->L, -1)) {
        lua_pop(impl_->L, 1);
        return;
    }

    auto* fn = new NativeFunction(std::move(func));
    impl_->native_functions.push_back(fn);
    lua_pushlightuserdata(impl_->L, fn);
    lua_pushcclosure(
        impl_->L,
        [](lua_State* ls) -> int {
            auto* f = static_cast<NativeFunction*>(lua_touserdata(ls, lua_upvalueindex(1)));
            return (*f)(ls);
        },
        1);
    lua_setfield(impl_->L, -2, name);
    lua_pop(impl_->L, 1);
}

lua_State* ScriptRuntime::state() const { return impl_->L; }

// ---------------------------------------------------------------------------
// Lua API bindings
// ---------------------------------------------------------------------------

void ScriptRuntime::Impl::RegisterApi() {
    lua_newtable(L);

    lua_pushcfunction(L, LuaUguiFind);
    lua_setfield(L, -2, "find");

    lua_pushcfunction(L, LuaUguiGetProp);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, LuaUguiSetProp);
    lua_setfield(L, -2, "set");

    lua_pushcfunction(L, LuaUguiLog);
    lua_setfield(L, -2, "log");

    lua_setglobal(L, "ugui");
}

int ScriptRuntime::Impl::LuaUguiFind(lua_State* L) {
    auto* rt = FromState(L);
    const char* name = luaL_checkstring(L, 1);

    auto it = rt->widget_registry.find(name);
    Widget* w = (it != rt->widget_registry.end()) ? it->second : nullptr;
    if (!w) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushstring(L, w->name().c_str());
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, w->id());
    lua_setfield(L, -2, "id");

    return 1;
}

int ScriptRuntime::Impl::LuaUguiGetProp(lua_State* L) {
    auto* rt = FromState(L);
    const char* name = luaL_checkstring(L, 1);
    const char* prop = luaL_checkstring(L, 2);

    auto it = rt->widget_registry.find(name);
    Widget* w = (it != rt->widget_registry.end()) ? it->second : nullptr;
    if (!w) {
        lua_pushnil(L);
        return 1;
    }

    const Style& s = w->style();

    if (strcmp(prop, "opacity") == 0)
        lua_pushnumber(L, s.opacity);
    else if (strcmp(prop, "visible") == 0)
        lua_pushboolean(L, s.visibility == Visibility::kVisible);
    else if (strcmp(prop, "font-size") == 0)
        lua_pushnumber(L, s.font_size);
    else if (strcmp(prop, "corner-radius") == 0)
        lua_pushnumber(L, s.corner_radius);
    else
        lua_pushnil(L);

    return 1;
}

int ScriptRuntime::Impl::LuaUguiSetProp(lua_State* L) {
    auto* rt = FromState(L);
    const char* name = luaL_checkstring(L, 1);
    const char* prop = luaL_checkstring(L, 2);

    auto it = rt->widget_registry.find(name);
    Widget* w = (it != rt->widget_registry.end()) ? it->second : nullptr;
    if (!w) {
        std::fprintf(stderr, "ultragui/lua: ugui.set — widget '%s' not found\n", name);
        return 0;
    }

    Style s = w->style();

    if (strcmp(prop, "opacity") == 0) {
        s.opacity = static_cast<f32>(luaL_checknumber(L, 3));
    } else if (strcmp(prop, "visible") == 0) {
        s.visibility = lua_toboolean(L, 3) ? Visibility::kVisible : Visibility::kHidden;
        w->set_style(s);
        return 0;
    } else if (strcmp(prop, "font-size") == 0) {
        s.font_size = static_cast<f32>(luaL_checknumber(L, 3));
    } else if (strcmp(prop, "corner-radius") == 0) {
        f32 r = static_cast<f32>(luaL_checknumber(L, 3));
        s.corner_radius = r;
        s.corner_radius_tl = r;
        s.corner_radius_tr = r;
        s.corner_radius_br = r;
        s.corner_radius_bl = r;
    } else if (strcmp(prop, "color") == 0) {
        const char* val = luaL_checkstring(L, 3);
        if (val[0] == '#' && strlen(val) == 7) {
            u32 hex = 0;
            std::from_chars(val + 1, val + 7, hex, 16);
            s.text_color = Color::FromHex(hex);
        }
    } else if (strcmp(prop, "background") == 0) {
        const char* val = luaL_checkstring(L, 3);
        if (strcmp(val, "transparent") == 0) {
            s.background = Color::Transparent();
        } else if (val[0] == '#' && strlen(val) == 7) {
            u32 hex = 0;
            std::from_chars(val + 1, val + 7, hex, 16);
            s.background = Color::FromHex(hex);
        }
    } else if (strcmp(prop, "text") == 0) {
        if (auto* text = dynamic_cast<Text*>(w)) {
            text->set_text(luaL_checkstring(L, 3));
            return 0;
        }
        if (auto* btn = dynamic_cast<Button*>(w)) {
            btn->set_label(luaL_checkstring(L, 3));
            return 0;
        }
    }

    w->set_style(s);
    return 0;
}

int ScriptRuntime::Impl::LuaUguiLog(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    std::printf("[ugui/lua] %s\n", msg);
    return 0;
}

} // namespace ugui
