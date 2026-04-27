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
  void (*on_update)(WidgetRegistry& world, Widget& w, f64 dt) = nullptr;
  bool (*on_scroll)(WidgetRegistry& world, Widget& w, Vec2 delta) = nullptr;
  // Runs after the base stored the layout result (rect/content_rect).
  void (*on_layout)(WidgetRegistry& world, Widget& w, const Rect& rect,
                    const Rect& content_rect) = nullptr;
  bool (*on_key_down)(WidgetRegistry& world, Widget& w, i32 key, i32 mods) =
      nullptr;
  bool (*on_char_input)(WidgetRegistry& world, Widget& w, u32 codepoint) =
      nullptr;
  bool (*consumes_text_input)(const Widget& w) = nullptr;
  void (*on_dismiss)(WidgetRegistry& world, Widget& w) = nullptr;
};

/// Resolve the behaviour table for a kind. Built-in tables are installed on
/// first use.
const WidgetVTable& WidgetVTableFor(WidgetKind kind);

/// Install (or replace) the behaviour table for a kind.
void SetWidgetVTable(WidgetKind kind, const WidgetVTable& vt);

}  // namespace ugui

#endif  // UGUI_WIDGETS_WIDGET_VTABLE_H_
