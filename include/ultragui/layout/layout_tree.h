#ifndef ULTRAGUI_LAYOUT_LAYOUT_TREE_H_
#define ULTRAGUI_LAYOUT_LAYOUT_TREE_H_

#include <ultragui/layout/layout.h>

#include <vector>

namespace ugui {

class Widget;

/// Build a LayoutNode array from a widget tree, run Yoga layout, and apply
/// results back to widgets. The scratch buffer is reused across frames to
/// avoid per-frame allocation.
void ComputeWidgetLayout(Widget* root, const LayoutViewport& vp,
                           LayoutEngine& engine, std::vector<LayoutNode>& scratch);

} // namespace ugui

#endif  // ULTRAGUI_LAYOUT_LAYOUT_TREE_H_
