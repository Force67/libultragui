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
  void (*draw)(WidgetRegistry& world, wid e, Renderer2D& r) = nullptr;
  void (*measure)(WidgetRegistry& world, wid e, f32& out_w,
                  f32& out_h) = nullptr;
  WidgetId (*hit_test)(WidgetRegistry& world, wid e, Vec2 point) = nullptr;
  bool (*on_click)(WidgetRegistry& world, wid e) = nullptr;
  void (*on_update)(WidgetRegistry& world, wid e, f64 dt) = nullptr;
  bool (*on_scroll)(WidgetRegistry& world, wid e, Vec2 delta) = nullptr;
  // Runs after PaintWidget stored the layout result (rect/content_rect).
  void (*on_layout)(WidgetRegistry& world, wid e, const Rect& rect,
                    const Rect& content_rect) = nullptr;
  bool (*on_key_down)(WidgetRegistry& world, wid e, i32 key,
                      i32 mods) = nullptr;
  bool (*on_char_input)(WidgetRegistry& world, wid e, u32 codepoint) = nullptr;
  bool (*consumes_text_input)(WidgetRegistry& world, wid e) = nullptr;
  void (*on_dismiss)(WidgetRegistry& world, wid e) = nullptr;
  // When true PaintWidget draws nothing and `draw` paints the whole widget (for
  // kinds that fully own their visuals, e.g. slider, toggle, scroll view).
  bool custom_paint = false;
};

/// Resolve the behaviour table for a kind. Built-in tables are installed on
/// first use.
UGUI_API const WidgetVTable& WidgetVTableFor(WidgetKind kind);

/// Install (or replace) the behaviour table for a kind.
UGUI_API void SetWidgetVTable(WidgetKind kind, const WidgetVTable& vt);

}  // namespace ugui

#endif  // UGUI_WIDGETS_WIDGET_VTABLE_H_
