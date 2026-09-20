#ifndef ULTRAGUI_LAYOUT_LAYOUT_TREE_H_
#define ULTRAGUI_LAYOUT_LAYOUT_TREE_H_

#include <ugui/core/handle.h>
#include <ugui/layout/layout.h>

namespace ugui {

/// Build a LayoutNode array from a widget tree, run Yoga layout, and apply
/// results back to the entities. The node array lives in the engine, one per
/// root, and is refreshed rather than rebuilt: a widget that has not been
/// marked dirty keeps the entry it had last frame.
void ComputeWidgetLayout(wid root, const LayoutViewport& vp,
                         LayoutEngine& engine);

}  // namespace ugui

#endif  // ULTRAGUI_LAYOUT_LAYOUT_TREE_H_
