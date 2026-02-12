#pragma once

namespace ugui {

class Widget;
class Renderer2D;

/// Depth-first paint pass: render widget backgrounds/text, then children.
/// Handles visibility culling and overflow scissoring.
void paint_widget_tree(Widget* root, Renderer2D& renderer);

} // namespace ugui
