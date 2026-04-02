#ifndef UGUI_CORE_HANDLE_H_
#define UGUI_CORE_HANDLE_H_

#include <ugui/core/types.h>

namespace ugui {

/// Stable, generation-checked handle to a widget (ECS-style id). Holding one of
/// these instead of a raw Widget* means a stale reference resolves to null
/// (via WidgetRegistry::Get) instead of dangling: when a widget is destroyed
/// its slot's generation is bumped, invalidating every outstanding handle.
///
/// index 0 is reserved as the null handle. The 32-bit generation does not
/// realistically wrap (a packed handle with a small generation would alias
/// under per-frame tree rebuilds, hence the full 8-byte handle).
struct WidgetId {
  u32 index = 0;
  u32 generation = 0;

  bool valid() const { return index != 0; }
  bool operator==(const WidgetId& o) const {
    return index == o.index && generation == o.generation;
  }
  bool operator!=(const WidgetId& o) const { return !(*this == o); }
};

/// Short alias for the widget handle. `wid` is the preferred spelling for
/// referring to a widget anywhere a raw Widget* used to be stored.
using wid = WidgetId;

inline constexpr WidgetId kNullWidget{};

struct WidgetIdHash {
  usize operator()(const WidgetId& id) const {
    return (static_cast<usize>(id.index) << 32) ^ id.generation;
  }
};

}  // namespace ugui

#endif  // UGUI_CORE_HANDLE_H_
