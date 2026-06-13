#include <ugui/scripting/lua_video.h>
#include <ugui/render/texture_backend.h>
#include <ugui/scripting/script_runtime.h>
#include <ugui/video/video.h>
#include <ugui/widgets/image.h>
#include <ugui/widgets/widget.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace ugui {

void RegisterVideoLua(ScriptRuntime& lua,
                      Function<VideoPlayer*(const char*)> loader,
                      Function<wid(const char*)> find_widget) {
  // ugui.load_video(path) -> video lightuserdata
  lua.RegisterFunction("load_video", [loader](lua_State* L) -> int {
    const char* path = luaL_checkstring(L, 1);
    VideoPlayer* vid = loader(path);
    if (vid)
      lua_pushlightuserdata(L, vid);
    else
      lua_pushnil(L);
    return 1;
  });

  // ugui.video_play(vid)
  lua.RegisterFunction("video_play", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    if (v) v->Play();
    return 0;
  });

  // ugui.video_pause(vid)
  lua.RegisterFunction("video_pause", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    if (v) v->Pause();
    return 0;
  });

  // ugui.video_stop(vid)
  lua.RegisterFunction("video_stop", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    if (v) v->Stop();
    return 0;
  });

  // ugui.video_seek(vid, seconds)
  lua.RegisterFunction("video_seek", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    if (v) v->Seek(luaL_checknumber(L, 2));
    return 0;
  });

  // ugui.video_loop(vid, bool)
  lua.RegisterFunction("video_loop", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    if (v) v->set_loop(lua_toboolean(L, 2));
    return 0;
  });

  // ugui.video_speed(vid, speed)
  lua.RegisterFunction("video_speed", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    if (v) v->set_speed(static_cast<f32>(luaL_checknumber(L, 2)));
    return 0;
  });

  // ugui.video_volume(vid, volume)
  lua.RegisterFunction("video_volume", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    if (v) v->set_volume(static_cast<f32>(luaL_checknumber(L, 2)));
    return 0;
  });

  // ugui.video_mute(vid, bool)
  lua.RegisterFunction("video_mute", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    if (v) v->set_muted(lua_toboolean(L, 2));
    return 0;
  });

  // ugui.video_playing(vid) -> bool
  lua.RegisterFunction("video_playing", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    lua_pushboolean(L, v && v->IsPlaying());
    return 1;
  });

  // ugui.video_finished(vid) -> bool
  lua.RegisterFunction("video_finished", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    lua_pushboolean(L, v && v->IsFinished());
    return 1;
  });

  // ugui.video_position(vid) -> number (seconds)
  lua.RegisterFunction("video_position", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    lua_pushnumber(L, v ? v->position() : 0);
    return 1;
  });

  // ugui.video_duration(vid) -> number (seconds)
  lua.RegisterFunction("video_duration", [](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    lua_pushnumber(L, v ? v->duration() : 0);
    return 1;
  });

  // ugui.video_attach(vid, "widget_name") -- set texture on an Image widget
  lua.RegisterFunction("video_attach", [find_widget](lua_State* L) -> int {
    auto* v = static_cast<VideoPlayer*>(lua_touserdata(L, 1));
    const char* name = luaL_checkstring(L, 2);
    if (v) {
      wid w = find_widget(name);
      if (w.valid())
        SetImageTexture(w, TextureIdFromRhiHandle(v->texture()),
                        static_cast<f32>(v->width()),
                        static_cast<f32>(v->height()));
    }
    return 0;
  });
}

}  // namespace ugui
