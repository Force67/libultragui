#pragma once

namespace ugui {

class LuaRuntime;
class AudioEngine;

/// Register audio Lua bindings (ugui.play_sound, ugui.stop_sound, etc.)
void register_audio_lua(LuaRuntime& lua, AudioEngine& audio);

} // namespace ugui
