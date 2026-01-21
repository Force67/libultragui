#pragma once

#include <ultragui/widgets/widget.h>

namespace ugui {

/// Container widget - equivalent to a <div> or Panel.
/// Lays out children according to its flex style.
class Panel : public Widget {
public:
    using Widget::Widget;

    void on_paint(Renderer2D& renderer) override;
};

} // namespace ugui
