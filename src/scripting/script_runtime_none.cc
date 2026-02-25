#include <ultragui/scripting/script_runtime.h>

namespace ugui {

struct ScriptRuntime::Impl {};

ScriptRuntime::ScriptRuntime() : impl_(new Impl()) {}
ScriptRuntime::~ScriptRuntime() { delete impl_; }

bool ScriptRuntime::Init() { return true; }
void ScriptRuntime::Shutdown() {}
bool ScriptRuntime::Exec(const char*, const char*) { return false; }
bool ScriptRuntime::ExecFile(const char*) { return false; }
void ScriptRuntime::RegisterWidget(Widget*) {}
void ScriptRuntime::UnregisterWidget(Widget*) {}
void ScriptRuntime::ClearWidgetRegistry() {}
bool ScriptRuntime::CallHandler(const char*, Widget*) { return false; }
Widget* ScriptRuntime::FindRegisteredWidget(const char*) const { return nullptr; }

} // namespace ugui
