#ifndef ULTRAGUI_LAYOUT_LAYOUT_TREE_H_
#define ULTRAGUI_LAYOUT_LAYOUT_TREE_H_

#include <ugui/core/handle.h>
#include <ugui/layout/layout.h>

namespace ugui {

/// Build a LayoutNode array from a widget tree, run Yoga layout, and apply
/// results back to the entities. The scratch buffer is reused across frames to
/// avoid per-frame allocation.
void ComputeWidgetLayout(wid root, const LayoutViewport& vp,
                         LayoutEngine& engine, Vector<LayoutNode>& scratch);

}  // namespace ugui

#endif  // ULTRAGUI_LAYOUT_LAYOUT_TREE_H_
