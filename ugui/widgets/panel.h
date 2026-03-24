#ifndef ULTRAGUI_WIDGETS_PANEL_H_
#define ULTRAGUI_WIDGETS_PANEL_H_

#include <ugui/widgets/widget.h>

namespace ugui {

/// Container widget: equivalent to a <div> or Panel.
/// Lays out children according to its flex style.
class Panel : public Widget {
 public:
  using Widget::Widget;

  void OnPaint(Renderer2D& renderer) override;
};

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_PANEL_H_
