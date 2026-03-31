#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

namespace ugui {

WidgetId WidgetRegistry::Acquire(Widget* w) {
  if (!w) return kNullWidget;

  // Already has a live slot pointing at this widget.
  if (w->self_.valid() && w->self_.index < slots_.size()) {
    Slot& s = slots_[w->self_.index];
    if (s.alive && s.ptr == w && s.generation == w->self_.generation)
      return w->self_;
  }

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
  s.ptr = w;
  s.alive = true;
  WidgetId id{index, s.generation};
  w->self_ = id;
  w->registry_ = this;
  return id;
}

Widget* WidgetRegistry::Get(WidgetId id) const {
  if (id.index == 0 || id.index >= slots_.size()) return nullptr;
  const Slot& s = slots_[id.index];
  if (!s.alive || s.generation != id.generation) return nullptr;
  return s.ptr;
}

void WidgetRegistry::Release(WidgetId id) {
  if (id.index == 0 || id.index >= slots_.size()) return;
  Slot& s = slots_[id.index];
  if (!s.alive || s.generation != id.generation) return;
  s.alive = false;
  s.ptr = nullptr;
  ++s.generation;
  if (s.generation == 0) s.generation = 1;
  free_.push_back(id.index);
}

}  // namespace ugui
