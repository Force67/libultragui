#ifndef ULTRAGUI_SCRIPTING_LUA_VIDEO_H_
#define ULTRAGUI_SCRIPTING_LUA_VIDEO_H_

#include <ugui/core/handle.h>
#include <ugui/core/types.h>

namespace ugui {

class ScriptRuntime;
class VideoPlayer;

/// Register Video Lua bindings (ugui.load_video, ugui.video_play, etc.)
/// The loader function creates and returns a VideoPlayer*.
/// The find_widget function locates widgets by name for video_attach.
void RegisterVideoLua(ScriptRuntime& rt,
                      Function<VideoPlayer*(const char*)> loader,
                      Function<wid(const char*)> find_widget);

}  // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_VIDEO_H_
