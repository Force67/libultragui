#include <ultragui/scripting/lua_runtime.h>
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

// Registry key for storing the LuaRuntime* in Lua's registry
static const char* const REGISTRY_KEY = "ugui_runtime";

LuaRuntime* LuaRuntime::FromState(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
    auto* rt = static_cast<LuaRuntime*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return rt;
}

bool LuaRuntime::Init() {
    L_ = luaL_newstate();
    if (!L_)
        return false;

    // Open safe standard libraries (no io/os for sandboxing)
    luaL_requiref(L_, "_G", luaopen_base, 1);
    lua_pop(L_, 1);
    luaL_requiref(L_, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L_, 1);
    luaL_requiref(L_, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L_, 1);
    luaL_requiref(L_, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L_, 1);

    // Store `this` in Lua's registry so static callbacks can find us
    lua_pushlightuserdata(L_, this);
    lua_setfield(L_, LUA_REGISTRYINDEX, REGISTRY_KEY);

    RegisterApi();
    return true;
}

void LuaRuntime::Shutdown() {
    if (L_) {
        lua_close(L_);
        L_ = nullptr;
    }
    widget_registry_.clear();
}

bool LuaRuntime::Exec(const char* script, const char* name) {
    if (luaL_loadbuffer(L_, script, strlen(script), name) != LUA_OK) {
        std::fprintf(stderr, "ultragui/lua: load error: %s\n", lua_tostring(L_, -1));
        lua_pop(L_, 1);
        return false;
    }
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        std::fprintf(stderr, "ultragui/lua: runtime error: %s\n", lua_tostring(L_, -1));
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

bool LuaRuntime::ExecFile(const char* path) {
    if (luaL_loadfile(L_, path) != LUA_OK) {
        std::fprintf(stderr, "ultragui/lua: failed to load '%s': %s\n", path, lua_tostring(L_, -1));
        lua_pop(L_, 1);
        return false;
    }
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        std::fprintf(stderr, "ultragui/lua: error in '%s': %s\n", path, lua_tostring(L_, -1));
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

void LuaRuntime::RegisterWidget(Widget* widget) {
    if (!widget->name().empty()) {
        widget_registry_[widget->name()] = widget;
    }
}

void LuaRuntime::UnregisterWidget(Widget* widget) {
    if (!widget->name().empty()) {
        widget_registry_.erase(widget->name());
    }
}

Widget* LuaRuntime::FindRegisteredWidget(const char* name) const {
    auto it = widget_registry_.find(name);
    return it != widget_registry_.end() ? it->second : nullptr;
}

bool LuaRuntime::CallHandler(const char* func_name, Widget* widget) {
    lua_getglobal(L_, func_name);
    if (!lua_isfunction(L_, -1)) {
        lua_pop(L_, 1);
        return false;
    }

    // Push widget as a table with name and id
    lua_newtable(L_);
    lua_pushstring(L_, widget->name().c_str());
    lua_setfield(L_, -2, "name");
    lua_pushinteger(L_, widget->id());
    lua_setfield(L_, -2, "id");

    if (lua_pcall(L_, 1, 0, 0) != LUA_OK) {
        std::fprintf(stderr, "ultragui/lua: error calling '%s': %s\n", func_name,
                     lua_tostring(L_, -1));
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

void LuaRuntime::RegisterFunction(const char* name, NativeFunction func) {
    lua_getglobal(L_, "ugui");
    if (!lua_istable(L_, -1)) {
        lua_pop(L_, 1);
        return;
    }

    auto* fn = new NativeFunction(std::move(func));
    lua_pushlightuserdata(L_, fn);
    lua_pushcclosure(
        L_,
        [](lua_State* ls) -> int {
            auto* f = static_cast<NativeFunction*>(lua_touserdata(ls, lua_upvalueindex(1)));
            return (*f)(ls);
        },
        1);
    lua_setfield(L_, -2, name);
    lua_pop(L_, 1);
}

// ---------------------------------------------------------------------------
// Lua API bindings
// ---------------------------------------------------------------------------

void LuaRuntime::RegisterApi() {
    lua_newtable(L_);

    lua_pushcfunction(L_, LuaUguiFind);
    lua_setfield(L_, -2, "find");

    lua_pushcfunction(L_, LuaUguiGetProp);
    lua_setfield(L_, -2, "get");

    lua_pushcfunction(L_, LuaUguiSetProp);
    lua_setfield(L_, -2, "set");

    lua_pushcfunction(L_, LuaUguiLog);
    lua_setfield(L_, -2, "log");

    lua_setglobal(L_, "ugui");
}

int LuaRuntime::LuaUguiFind(lua_State* L) {
    auto* rt = FromState(L);
    const char* name = luaL_checkstring(L, 1);

    Widget* w = rt->FindRegisteredWidget(name);
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

int LuaRuntime::LuaUguiGetProp(lua_State* L) {
    auto* rt = FromState(L);
    const char* name = luaL_checkstring(L, 1);
    const char* prop = luaL_checkstring(L, 2);

    Widget* w = rt->FindRegisteredWidget(name);
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

int LuaRuntime::LuaUguiSetProp(lua_State* L) {
    auto* rt = FromState(L);
    const char* name = luaL_checkstring(L, 1);
    const char* prop = luaL_checkstring(L, 2);

    Widget* w = rt->FindRegisteredWidget(name);
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

int LuaRuntime::LuaUguiLog(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    std::printf("[ugui/lua] %s\n", msg);
    return 0;
}

} // namespace ugui
