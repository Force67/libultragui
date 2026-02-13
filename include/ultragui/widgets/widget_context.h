#pragma once

#include <ultragui/core/types.h>
#include <ultragui/text/text_engine.h>

namespace ugui {

class Animator;

/// Shared context propagated through the widget tree.
/// Provides widgets with access to subsystems they need (text shaping, etc.)
/// without requiring manual per-widget injection.
struct WidgetContext {
    TextEngine* text_engine = nullptr;
    FontHandle default_font = INVALID_FONT;
    Animator* animator = nullptr;
    f64* current_time = nullptr;
};

} // namespace ugui
