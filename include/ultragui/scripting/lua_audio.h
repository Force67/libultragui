#ifndef ULTRAGUI_SCRIPTING_LUA_AUDIO_H_
#define ULTRAGUI_SCRIPTING_LUA_AUDIO_H_

namespace ugui {

class ScriptRuntime;
class AudioEngine;

/// Register audio Lua bindings (ugui.play_sound, ugui.stop_sound, etc.)
void RegisterAudioLua(ScriptRuntime& rt, AudioEngine& audio);

} // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_LUA_AUDIO_H_
