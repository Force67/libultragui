#ifndef ULTRAGUI_SCRIPTING_LUA_AUDIO_H_
#define ULTRAGUI_SCRIPTING_LUA_AUDIO_H_

namespace ugui {

class ScriptRuntime;
class AudioBackend;

/// Register audio Lua bindings (ugui.play_sound, ugui.stop_sound, etc.)
void RegisterAudioLua(ScriptRuntime& rt, AudioBackend& audio);

}  // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_AUDIO_H_
