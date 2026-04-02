#ifndef UGUI_WIDGETS_WIDGET_REGISTRY_H_
#define UGUI_WIDGETS_WIDGET_REGISTRY_H_

#include <ugui/core/config.h>
#include <ugui/core/handle.h>
#include <ugui/core/types.h>
#include <ugui/widgets/widget.h>

namespace ugui {

/// Generation-checked slot map that hands out stable WidgetId handles for live
/// widgets. The widget tree still owns its widgets (destruction is unchanged),
/// but parent/child links are stored as handles that resolve through this
/// registry, so any stored reference safely resolves to null once the widget is
/// gone. Every widget Acquire()s a slot eagerly in its constructor (via the
/// active registry), and the slot is freed (generation bumped) in the
/// destructor.
class WidgetRegistry {
 public:
  /// Return the widget's handle, allocating a slot on first use. Idempotent.
  WidgetId Acquire(Widget* w);

  /// Resolve a handle to a live widget, or nullptr if it was destroyed.
  Widget* Get(WidgetId id) const;

  /// Resolve + kind-checked downcast (no RTTI). Null if stale or wrong kind.
  template <class T>
  T* GetAs(WidgetId id) const {
    return widget_cast<T>(Get(id));
  }

  /// True if the handle still refers to a live widget.
  bool Alive(WidgetId id) const { return Get(id) != nullptr; }

  /// Release a slot (called from ~Widget). Bumps the generation so outstanding
  /// handles to this slot become stale.
  void Release(WidgetId id);

  /// Registry that new widgets register themselves into: the active context's,
  /// or a process-global default for widgets created without a UIContext.
  /// Every Widget acquires a handle from this in its constructor, so the tree
  /// can reference children/parents by handle instead of raw pointers.
  static WidgetRegistry* Active();

  /// RAII scope that makes `r` the active registry on this thread. UIContext
  /// wraps tree creation/update in one of these.
  struct ScopedActive {
    explicit ScopedActive(WidgetRegistry* r);
    ~ScopedActive();
    ScopedActive(const ScopedActive&) = delete;
    ScopedActive& operator=(const ScopedActive&) = delete;

   private:
    WidgetRegistry* prev_;
  };

 private:
  struct Slot {
    Widget* ptr = nullptr;
    u32 generation = 1;  // never 0 so a live handle is always valid()
    bool alive = false;
  };
  Vector<Slot> slots_;  // slots_[0] is the reserved null slot
  Vector<u32> free_;    // indices available for reuse
};

}  // namespace ugui

#endif  // UGUI_WIDGETS_WIDGET_REGISTRY_H_
