#include <ugui/scripting/script_runtime.h>

namespace ugui {

struct ScriptRuntime::Impl {};

ScriptRuntime::ScriptRuntime() : impl_(new Impl()) {}
ScriptRuntime::~ScriptRuntime() { delete impl_; }

bool ScriptRuntime::Init() { return true; }
void ScriptRuntime::Shutdown() {}
bool ScriptRuntime::Exec(const char*, const char*) { return false; }
bool ScriptRuntime::ExecFile(const char*) { return false; }
void ScriptRuntime::RegisterWidget(wid) {}
void ScriptRuntime::UnregisterWidget(wid) {}
void ScriptRuntime::ClearWidgetRegistry() {}
bool ScriptRuntime::CallHandler(const char*, wid) { return false; }
wid ScriptRuntime::FindRegisteredWidget(const char*) const {
  return kNullWidget;
}
void ScriptRuntime::UpdateTimers(double) {}
void ScriptRuntime::SyncTimerClock(double) {}
void ScriptRuntime::WireChangeHandlers(wid) {}
void ScriptRuntime::ClearTimersAndTweens() {}

}  // namespace ugui
