#ifndef UGUI_CORE_COMPONENT_STORE_H_
#define UGUI_CORE_COMPONENT_STORE_H_

#include <ugui/core/config.h>
#include <ugui/core/handle.h>
#include <ugui/core/types.h>

namespace ugui {

/// Monotonic per-type id for components, assigned on first use so a World can
/// look up each store in O(1). Header-only, no RTTI; host-defined component
/// types get a fresh id automatically.
inline u32 NextComponentTypeId() {
  static u32 counter = 0;
  return counter++;
}
template <class C>
u32 ComponentTypeId() {
  static const u32 id = NextComponentTypeId();
  return id;
}

/// Type-erased base so a World can own a heterogeneous set of stores and drop
/// an entity's components when it is released.
class IComponentStore {
 public:
  virtual ~IComponentStore() = default;
  virtual void Remove(WidgetId id) = 0;
};

/// Sparse-set storage for one component type. Dense data for cache-friendly
/// iteration; `sparse_` maps entity index to dense slot. Lookups are
/// generation-checked, so a stale handle never reads a reused entity's slot.
///
/// Add/Remove may move elements (swap-and-pop): never hold a C* across a
/// structural change.
template <class C>
class ComponentStore : public IComponentStore {
 public:
  /// Attach (or overwrite) the component for `id`. Returns a reference valid
  /// until the next structural change.
  C& Add(WidgetId id, C value) {
    EnsureSparse(id.index);
    u32 d = sparse_[id.index];
    if (d != kInvalid && d < dense_ids_.size() && dense_ids_[d] == id) {
      data_[d] = ugui::move(value);
      return data_[d];
    }
    sparse_[id.index] = static_cast<u32>(data_.size());
    dense_ids_.push_back(id);
    data_.push_back(ugui::move(value));
    return data_.back();
  }

  /// Resolve the component for `id`, or nullptr if absent / handle stale.
  C* Get(WidgetId id) {
    if (id.index >= sparse_.size()) return nullptr;
    u32 d = sparse_[id.index];
    if (d == kInvalid || d >= dense_ids_.size() || dense_ids_[d] != id)
      return nullptr;
    return &data_[d];
  }

  bool Has(WidgetId id) { return Get(id) != nullptr; }

  void Remove(WidgetId id) override {
    if (id.index >= sparse_.size()) return;
    u32 d = sparse_[id.index];
    if (d == kInvalid || d >= dense_ids_.size() || dense_ids_[d] != id) return;
    u32 last = static_cast<u32>(data_.size() - 1);
    data_[d] = ugui::move(data_[last]);
    dense_ids_[d] = dense_ids_[last];
    sparse_[dense_ids_[d].index] = d;
    data_.pop_back();
    dense_ids_.pop_back();
    sparse_[id.index] = kInvalid;
  }

  // --- Dense iteration for systems ---
  usize size() const { return data_.size(); }
  const Vector<WidgetId>& ids() const { return dense_ids_; }
  typename Vector<C>::iterator begin() { return data_.begin(); }
  typename Vector<C>::iterator end() { return data_.end(); }

 private:
  static constexpr u32 kInvalid = ~0u;
  void EnsureSparse(u32 index) {
    if (index >= sparse_.size()) sparse_.resize(index + 1, kInvalid);
  }
  Vector<C> data_;
  Vector<WidgetId> dense_ids_;
  Vector<u32> sparse_;
};

}  // namespace ugui

#endif  // UGUI_CORE_COMPONENT_STORE_H_
