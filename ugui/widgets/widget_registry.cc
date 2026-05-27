#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

namespace ugui {

namespace {
thread_local WidgetRegistry* t_active = nullptr;
}  // namespace

WidgetRegistry* WidgetRegistry::Active() {
  if (t_active) return t_active;
  static WidgetRegistry s_default;  // for entities created without a UIContext
  return &s_default;
}

WidgetRegistry::ScopedActive::ScopedActive(WidgetRegistry* r) : prev_(t_active) {
  t_active = r;
}
WidgetRegistry::ScopedActive::~ScopedActive() { t_active = prev_; }

wid WidgetRegistry::New(u32 id) {
  if (slots_.empty()) slots_.push_back(Slot{});  // reserve index 0 as null

  u32 index;
  if (!free_.empty()) {
    index = free_.back();
    free_.pop_back();
  } else {
    index = static_cast<u32>(slots_.size());
    slots_.push_back(Slot{});
  }

  Slot& s = slots_[index];
  s.alive = true;
  wid e{index, s.generation};

  // Attach the core components every widget entity carries.
  Add<WidgetNode>(e, WidgetNode{id == 0 ? NextWidgetId() : id});
  Add<Transform>(e, Transform{});
  Add<StyleC>(e, StyleC{});
  Add<Hierarchy>(e, Hierarchy{});
  return e;
}

bool WidgetRegistry::Alive(WidgetId id) const {
  if (id.index == 0 || id.index >= slots_.size()) return false;
  const Slot& s = slots_[id.index];
  return s.alive && s.generation == id.generation;
}

void WidgetRegistry::Release(WidgetId id) {
  if (id.index == 0 || id.index >= slots_.size()) return;
  Slot& s = slots_[id.index];
  if (!s.alive || s.generation != id.generation) return;
  for (auto& store : stores_)
    if (store) store->Remove(id);
  s.alive = false;
  ++s.generation;
  if (s.generation == 0) s.generation = 1;
  free_.push_back(id.index);
}

}  // namespace ugui
