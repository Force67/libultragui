#ifndef ULTRAGUI_SCRIPTING_SCRIPT_RUNTIME_H_
#define ULTRAGUI_SCRIPTING_SCRIPT_RUNTIME_H_

#include <ugui/ultragui_config.h>

#if ULTRAGUI_LUA
#include <ugui/core/types.h>
struct lua_State;
#endif

namespace ugui {

class Widget;

/// Concrete scripting runtime with link-time swappable implementation.
/// The default implementation uses Lua 5.4 (when ULTRAGUI_LUA=1);
/// a no-op stub is used when scripting is disabled.
/// Swap the .cc file via CMake (ULTRAGUI_SCRIPT_SOURCE) to provide
/// your own scripting backend.
class ScriptRuntime {
 public:
  ScriptRuntime();
  ~ScriptRuntime();
  ScriptRuntime(const ScriptRuntime&) = delete;
  ScriptRuntime& operator=(const ScriptRuntime&) = delete;

  bool Init();
  void Shutdown();

  /// Execute a script from a string.
  bool Exec(const char* script, const char* name = "chunk");

  /// Execute a script from a file path.
  bool ExecFile(const char* path);

  /// Register a widget so the scripting layer can find it by name.
  void RegisterWidget(Widget* widget);
  void UnregisterWidget(Widget* widget);
  void ClearWidgetRegistry();

  /// Call a named handler function, passing the widget as context.
  /// The widget table includes type-specific fields (checked, selected, value).
  /// Returns true if the handler existed and ran successfully.
  bool CallHandler(const char* func_name, Widget* widget);

  /// Look up a registered widget by name.
  Widget* FindRegisteredWidget(const char* name) const;

  /// Schedule a Lua callback to fire after `delay_seconds`.
  /// The callback is a Lua function reference stored via luaL_ref.
  void ScheduleTimer(double delay_seconds, int lua_func_ref);

  /// Sync the timer clock without firing callbacks.
  /// Call before executing scripts so ugui.after() uses real time.
  void SyncTimerClock(double current_time);

  /// Cancel all pending timers and tweens (call when replacing the UI tree).
  void ClearTimersAndTweens();

  /// Process pending timers and tweens. Call each frame with the current time.
  void UpdateTimers(double current_time);

  /// Auto-wire on_change callbacks for Dropdown/Checkbox/Slider widgets
  /// in the tree so they dispatch to Lua on_<name>(w) handlers.
  void WireChangeHandlers(Widget* root);

#if ULTRAGUI_LUA
  /// Expose a C++ function to Lua under ugui.{name}. Lua-specific.
  using NativeFunction = Function<int(lua_State*)>;
  void RegisterFunction(const char* name, NativeFunction func);

  lua_State* state() const;
#endif

  struct Impl;

 private:
  Impl* impl_;
};

}  // namespace ugui

#endif  // ULTRAGUI_SCRIPTING_SCRIPT_RUNTIME_H_
