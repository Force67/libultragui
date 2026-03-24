#ifndef ULTRAGUI_RENDER_PAINT_H_
#define ULTRAGUI_RENDER_PAINT_H_

namespace ugui {

class Widget;
class Renderer2D;

/// Depth-first paint pass: render widget backgrounds/text, then children.
/// Handles visibility culling and overflow scissoring.
void PaintWidgetTree(Widget* root, Renderer2D& renderer);

}  // namespace ugui

#endif  // ULTRAGUI_RENDER_PAINT_H_
