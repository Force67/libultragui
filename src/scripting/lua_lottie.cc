#include <ultragui/scripting/lua_lottie.h>
#include <ultragui/lottie/lottie.h>
#include <ultragui/scripting/script_runtime.h>
#include <ultragui/widgets/image.h>
#include <ultragui/widgets/widget.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace ugui {

void RegisterLottieLua(
    ScriptRuntime& lua,
    Function<LottieAnimation*(const char*, unsigned, unsigned)> loader,
    Function<Widget*(const char*)> find_widget) {

    // ugui.LoadLottie(path, width, height) -> anim lightuserdata
    lua.RegisterFunction("load_lottie", [loader](lua_State* L) -> int {
        const char* path = luaL_checkstring(L, 1);
        u32 w = static_cast<u32>(luaL_checkinteger(L, 2));
        u32 h = static_cast<u32>(luaL_checkinteger(L, 3));
        LottieAnimation* anim = loader(path, w, h);
        if (anim)
            lua_pushlightuserdata(L, anim);
        else
            lua_pushnil(L);
        return 1;
    });

    // ugui.lottie_play(anim)
    lua.RegisterFunction("lottie_play", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->Play();
        return 0;
    });

    // ugui.lottie_pause(anim)
    lua.RegisterFunction("lottie_pause", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->Pause();
        return 0;
    });

    // ugui.lottie_stop(anim)
    lua.RegisterFunction("lottie_stop", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->Stop();
        return 0;
    });

    // ugui.lottie_loop(anim, bool)
    lua.RegisterFunction("lottie_loop", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->set_loop(lua_toboolean(L, 2));
        return 0;
    });

    // ugui.lottie_speed(anim, speed)
    lua.RegisterFunction("lottie_speed", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->set_speed(static_cast<f32>(luaL_checknumber(L, 2)));
        return 0;
    });

    // ugui.lottie_seek(anim, progress) -- 0.0 to 1.0
    lua.RegisterFunction("lottie_seek", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        if (a) a->Seek(static_cast<f32>(luaL_checknumber(L, 2)));
        return 0;
    });

    // ugui.lottie_playing(anim) -> bool
    lua.RegisterFunction("lottie_playing", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        lua_pushboolean(L, a && a->IsPlaying());
        return 1;
    });

    // ugui.lottie_progress(anim) -> number
    lua.RegisterFunction("lottie_progress", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        lua_pushnumber(L, a ? a->progress() : 0);
        return 1;
    });

    // ugui.lottie_duration(anim) -> number (seconds)
    lua.RegisterFunction("lottie_duration", [](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
        lua_pushnumber(L, a ? a->duration() : 0);
        return 1;
    });

    // ugui.lottie_attach(anim, "widget_name") -- set texture on an Image widget
    lua.RegisterFunction("lottie_attach", [find_widget](lua_State* L) -> int {
        auto* a = static_cast<LottieAnimation*>(lua_touserdata(L, 1));
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

} // namespace ugui
