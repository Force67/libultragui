#include <ultragui/scripting/lua_audio.h>
#include <ultragui/audio/audio.h>
#include <ultragui/scripting/lua_runtime.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace ugui {

void register_audio_lua(LuaRuntime& lua, AudioEngine& audio) {
    // ugui.play_sound(path [, volume [, loop]]) -> handle
    lua.register_function("play_sound", [&audio](lua_State* L) -> int {
        const char* path = luaL_checkstring(L, 1);
        f32 volume = lua_isnumber(L, 2) ? static_cast<f32>(lua_tonumber(L, 2)) : 1.0f;
        bool loop = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : false;
        SoundHandle h = audio.play(path, volume, loop);
        lua_pushinteger(L, h);
        return 1;
    });

    // ugui.load_sound(path) -> handle
    lua.register_function("load_sound", [&audio](lua_State* L) -> int {
        const char* path = luaL_checkstring(L, 1);
        SoundHandle h = audio.load(path);
        lua_pushinteger(L, h);
        return 1;
    });

    // ugui.play_loaded(handle [, volume [, loop]]) -> new handle
    lua.register_function("play_loaded", [&audio](lua_State* L) -> int {
        SoundHandle src = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        f32 volume = lua_isnumber(L, 2) ? static_cast<f32>(lua_tonumber(L, 2)) : 1.0f;
        bool loop = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : false;
        SoundHandle h = audio.play_loaded(src, volume, loop);
        lua_pushinteger(L, h);
        return 1;
    });

    // ugui.stop_sound(handle)
    lua.register_function("stop_sound", [&audio](lua_State* L) -> int {
        SoundHandle h = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        audio.stop(h);
        return 0;
    });

    // ugui.sound_playing(handle) -> bool
    lua.register_function("sound_playing", [&audio](lua_State* L) -> int {
        SoundHandle h = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        lua_pushboolean(L, audio.is_playing(h));
        return 1;
    });

    // ugui.sound_volume(handle, volume)
    lua.register_function("sound_volume", [&audio](lua_State* L) -> int {
        SoundHandle h = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        f32 vol = static_cast<f32>(luaL_checknumber(L, 2));
        audio.set_volume(h, vol);
        return 0;
    });

    // ugui.sound_pan(handle, pan)
    lua.register_function("sound_pan", [&audio](lua_State* L) -> int {
        SoundHandle h = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        f32 pan = static_cast<f32>(luaL_checknumber(L, 2));
        audio.set_pan(h, pan);
        return 0;
    });

    // ugui.sound_pitch(handle, pitch)
    lua.register_function("sound_pitch", [&audio](lua_State* L) -> int {
        SoundHandle h = static_cast<SoundHandle>(luaL_checkinteger(L, 1));
        f32 pitch = static_cast<f32>(luaL_checknumber(L, 2));
        audio.set_pitch(h, pitch);
        return 0;
    });

    // ugui.master_volume(volume) or ugui.master_volume() -> number
    lua.register_function("master_volume", [&audio](lua_State* L) -> int {
        if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
            audio.set_master_volume(static_cast<f32>(lua_tonumber(L, 1)));
            return 0;
        }
        lua_pushnumber(L, audio.master_volume());
        return 1;
    });

    // ugui.stop_all_sounds()
    lua.register_function("stop_all_sounds", [&audio](lua_State*) -> int {
        audio.stop_all();
        return 0;
    });

    // ugui.pause_all_sounds()
    lua.register_function("pause_all_sounds", [&audio](lua_State*) -> int {
        audio.pause_all();
        return 0;
    });

    // ugui.resume_all_sounds()
    lua.register_function("resume_all_sounds", [&audio](lua_State*) -> int {
        audio.resume_all();
        return 0;
    });
}

} // namespace ugui
