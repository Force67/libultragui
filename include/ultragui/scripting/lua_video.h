#ifndef ULTRAGUI_SCRIPTING_LUA_VIDEO_H_
#define ULTRAGUI_SCRIPTING_LUA_VIDEO_H_

#include <ultragui/core/types.h>

namespace ugui {

class LuaRuntime;
class VideoPlayer;
class Widget;

/// Register Video Lua bindings (ugui.load_video, ugui.video_play, etc.)
/// The loader function creates and returns a VideoPlayer*.
/// The find_widget function locates widgets by name for video_attach.
void RegisterVideoLua(
    LuaRuntime& lua,
    Function<VideoPlayer*(const char*)> loader,
    Function<Widget*(const char*)> find_widget);

} // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_VIDEO_H_
