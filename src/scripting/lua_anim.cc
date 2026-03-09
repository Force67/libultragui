#include <ultragui/anim/vector_animation.h>
#include <ultragui/scripting/lua_anim.h>
#include <ultragui/scripting/script_runtime.h>
#include <ultragui/widgets/image.h>
#include <ultragui/widgets/widget.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace ugui {

void RegisterAnimLua(
    ScriptRuntime& lua,
    Function<VectorAnimation*(const char*, unsigned, unsigned)> loader,
    Function<Widget*(const char*)> find_widget) {
  lua.RegisterFunction("load_anim", [loader](lua_State* L) -> int {
    const char* path = luaL_checkstring(L, 1);
    u32 w = static_cast<u32>(luaL_checkinteger(L, 2));
    u32 h = static_cast<u32>(luaL_checkinteger(L, 3));
    VectorAnimation* anim = loader(path, w, h);
    if (anim)
      lua_pushlightuserdata(L, anim);
    else
      lua_pushnil(L);
    return 1;
  });

  lua.RegisterFunction("anim_play", [](lua_State* L) -> int {
    auto* a = static_cast<VectorAnimation*>(lua_touserdata(L, 1));
    if (a) a->Play();
    return 0;
  });

  lua.RegisterFunction("anim_pause", [](lua_State* L) -> int {
    auto* a = static_cast<VectorAnimation*>(lua_touserdata(L, 1));
    if (a) a->Pause();
    return 0;
  });

  lua.RegisterFunction("anim_stop", [](lua_State* L) -> int {
    auto* a = static_cast<VectorAnimation*>(lua_touserdata(L, 1));
    if (a) a->Stop();
    return 0;
  });

  lua.RegisterFunction("anim_loop", [](lua_State* L) -> int {
    auto* a = static_cast<VectorAnimation*>(lua_touserdata(L, 1));
    if (a) a->set_loop(lua_toboolean(L, 2));
    return 0;
  });

  lua.RegisterFunction("anim_speed", [](lua_State* L) -> int {
    auto* a = static_cast<VectorAnimation*>(lua_touserdata(L, 1));
    if (a) a->set_speed(static_cast<f32>(luaL_checknumber(L, 2)));
    return 0;
  });

  lua.RegisterFunction("anim_seek", [](lua_State* L) -> int {
    auto* a = static_cast<VectorAnimation*>(lua_touserdata(L, 1));
    if (a) a->Seek(static_cast<f32>(luaL_checknumber(L, 2)));
    return 0;
  });

  lua.RegisterFunction("anim_playing", [](lua_State* L) -> int {
    auto* a = static_cast<VectorAnimation*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a && a->IsPlaying());
    return 1;
  });

  lua.RegisterFunction("anim_progress", [](lua_State* L) -> int {
    auto* a = static_cast<VectorAnimation*>(lua_touserdata(L, 1));
    lua_pushnumber(L, a ? a->progress() : 0);
    return 1;
  });

  lua.RegisterFunction("anim_duration", [](lua_State* L) -> int {
    auto* a = static_cast<VectorAnimation*>(lua_touserdata(L, 1));
    lua_pushnumber(L, a ? a->duration() : 0);
    return 1;
  });

  lua.RegisterFunction("anim_attach", [find_widget](lua_State* L) -> int {
    auto* a = static_cast<VectorAnimation*>(lua_touserdata(L, 1));
    const char* name = luaL_checkstring(L, 2);
    if (a) {
      Widget* w = find_widget(name);
      if (auto* img = dynamic_cast<Image*>(w)) {
        img->set_texture(a->texture(), static_cast<f32>(a->width()),
                         static_cast<f32>(a->height()));
      }
    }
    return 0;
  });
}

}  // namespace ugui
