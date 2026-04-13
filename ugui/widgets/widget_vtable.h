#ifndef UGUI_WIDGETS_WIDGET_VTABLE_H_
#define UGUI_WIDGETS_WIDGET_VTABLE_H_

#include <ugui/core/handle.h>
#include <ugui/core/math.h>
#include <ugui/core/types.h>
#include <ugui/widgets/widget.h>

namespace ugui {

class Renderer2D;
class WidgetRegistry;  // == World

/// Per-kind behaviour table: the composition-lite replacement for per-widget
/// virtual overrides. A generic Widget tagged with a WidgetKind dispatches its
/// paint / measure / hit-test / click to that kind's entry, which reads the
/// widget's components from the World. Entries left null mean "no custom
/// behaviour" and the Widget base handles it (e.g. a plain panel).
struct WidgetVTable {
  void (*draw)(WidgetRegistry& world, Widget& w, Renderer2D& r) = nullptr;
  void (*measure)(WidgetRegistry& world, Widget& w, f32& out_w, f32& out_h) =
      nullptr;
  WidgetId (*hit_test)(WidgetRegistry& world, Widget& w, Vec2 point) = nullptr;
  bool (*on_click)(WidgetRegistry& world, Widget& w) = nullptr;
};

/// Resolve the behaviour table for a kind. Built-in tables are installed on
/// first use.
const WidgetVTable& WidgetVTableFor(WidgetKind kind);

/// Install (or replace) the behaviour table for a kind.
void SetWidgetVTable(WidgetKind kind, const WidgetVTable& vt);

}  // namespace ugui

#endif  // UGUI_WIDGETS_WIDGET_VTABLE_H_
