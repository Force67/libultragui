#pragma once

#include <ultragui/layout/layout.h>

#include <vector>

namespace ugui {

class Widget;

/// Build a LayoutNode array from a widget tree, run Yoga layout, and apply
/// results back to widgets. The scratch buffer is reused across frames to
/// avoid per-frame allocation.
void compute_widget_layout(Widget* root, const LayoutViewport& vp,
                           LayoutEngine& engine, std::vector<LayoutNode>& scratch);

} // namespace ugui
