#ifndef UGUI_WIDGETS_WIDGET_REGISTRY_H_
#define UGUI_WIDGETS_WIDGET_REGISTRY_H_

#include <memory>

#include <ugui/core/component_store.h>
#include <ugui/core/config.h>
#include <ugui/core/handle.h>
#include <ugui/core/types.h>

namespace ugui {

/// The entity-and-component world: a generation-checked slot map of widget
/// entities plus their component stores. An entity (WidgetId / wid) owns no
/// object; its data lives in components. New() allocates an entity with the
/// core widget components; Release() frees it (dropping all its components and
/// bumping the generation so stale handles resolve to dead).
class WidgetRegistry {
 public:
  /// Allocate a new widget entity with the core components (WidgetNode,
  /// Transform, StyleC, Hierarchy). `id` is the application id (0 = auto).
  wid New(u32 id = 0);

  /// True if the handle still refers to a live entity.
  bool Alive(WidgetId id) const;

  /// Free an entity: drop its components and bump the generation.
  void Release(WidgetId id);

  // --- Components (composition-lite ECS) ---------------------------------
  // An entity can carry any set of component structs. Host engines attach
  // their own types the same way, no core changes needed:
  //   world.Add<MyComponent>(id, {...});
  //   if (auto* c = world.Get<MyComponent>(id)) ...
  // Components are dropped automatically when the entity is released.

  /// The store for component type C, created on first use.
  template <class C>
  ComponentStore<C>& Store() {
    u32 tid = ComponentTypeId<C>();
    if (tid >= stores_.size()) stores_.resize(tid + 1);
    if (!stores_[tid]) stores_[tid] = std::make_unique<ComponentStore<C>>();
    return *static_cast<ComponentStore<C>*>(stores_[tid].get());
  }
  template <class C>
  C& Add(WidgetId id, C value) {
    return Store<C>().Add(id, std::move(value));
  }
  /// Get the component for `id`, creating a default-constructed one if absent.
  template <class C>
  C& GetOrAdd(WidgetId id) {
    ComponentStore<C>& s = Store<C>();
    C* c = s.Get(id);
    return c ? *c : s.Add(id, C{});
  }
  template <class C>
  C* Get(WidgetId id) {
    return Store<C>().Get(id);
  }
  template <class C>
  bool Has(WidgetId id) {
    return Store<C>().Has(id);
  }
  template <class C>
  void Remove(WidgetId id) {
    Store<C>().Remove(id);
  }

  /// Registry that new entities are created into: the active context's, or a
  /// process-global default for entities created without a UIContext.
  static WidgetRegistry* Active();

  /// RAII scope that makes `r` the active registry on this thread. UIContext
  /// wraps its whole lifetime in one of these.
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
    u32 generation = 1;  // never 0 so a live handle is always valid()
    bool alive = false;
  };
  Vector<Slot> slots_;  // slots_[0] is the reserved null slot
  Vector<u32> free_;    // indices available for reuse
  Vector<std::unique_ptr<IComponentStore>> stores_;  // indexed by component id
};

/// The registry is the entity-and-component world.
using World = WidgetRegistry;

}  // namespace ugui

#endif  // UGUI_WIDGETS_WIDGET_REGISTRY_H_
