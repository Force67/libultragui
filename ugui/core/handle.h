#ifndef UGUI_CORE_HANDLE_H_
#define UGUI_CORE_HANDLE_H_

#include <ugui/core/types.h>

namespace ugui {

/// Generation-checked widget handle (ECS-style id). A stale handle resolves
/// to null via WidgetRegistry::Get instead of dangling: destroying a widget
/// bumps its slot's generation, invalidating outstanding handles.
///
/// index 0 is the null handle. The full 8-byte handle keeps the generation
/// wide so it cannot alias under per-frame tree rebuilds.
struct WidgetId {
  u32 index = 0;
  u32 generation = 0;

  bool valid() const { return index != 0; }
  bool operator==(const WidgetId& o) const {
    return index == o.index && generation == o.generation;
  }
  bool operator!=(const WidgetId& o) const { return !(*this == o); }
};

/// Preferred spelling for a widget handle.
using wid = WidgetId;

inline constexpr WidgetId kNullWidget{};

struct WidgetIdHash {
  usize operator()(const WidgetId& id) const {
    return (static_cast<usize>(id.index) << 32) ^ id.generation;
  }
};

}  // namespace ugui

#endif  // UGUI_CORE_HANDLE_H_
