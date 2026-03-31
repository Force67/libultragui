#ifndef ULTRAGUI_WIDGETS_WIDGET_CONTEXT_H_
#define ULTRAGUI_WIDGETS_WIDGET_CONTEXT_H_

#include <ugui/core/types.h>
#include <ugui/text/text_engine.h>

namespace ugui {

class Animator;
class Platform;
class WidgetRegistry;

/// Shared context propagated through the widget tree.
/// Provides widgets with access to subsystems they need (text shaping, etc.)
/// without requiring manual per-widget injection.
struct WidgetContext {
  TextEngine* text_engine = nullptr;
  FontHandle default_font = kInvalidFont;
  Animator* animator = nullptr;
  f64* current_time = nullptr;
  Platform* platform = nullptr;
  WidgetRegistry* registry = nullptr;  ///< for Widget::handle() (stable ids)
  f32 ui_scale = 1.0f;  ///< Viewport scale factor (1.0 = design resolution)
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_WIDGET_CONTEXT_H_
