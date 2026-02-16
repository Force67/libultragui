#ifndef ULTRAGUI_LAYOUT_LAYOUT_H_
#define ULTRAGUI_LAYOUT_LAYOUT_H_

#include <ultragui/core/rect.h>
#include <ultragui/core/types.h>
#include <ultragui/style/style.h>

#include <vector>

namespace ugui {

/// A node in the layout tree. Each node has a Style and produces a computed Rect.
/// This is the input/output structure for the layout algorithm.
struct LayoutNode {
    // Input
    Style style;
    u32 id = 0; // Widget ID for mapping back

    // Intrinsic content size (e.g. text bounding box, image size)
    f32 intrinsic_width = 0.0f;
    f32 intrinsic_height = 0.0f;

    // Tree structure (indices into the LayoutContext's node array)
    u32 parent = ~0u;
    u32 first_child = ~0u;
    u32 next_sibling = ~0u;
    u32 child_count = 0;

    // Output (computed by layout)
    Rect computed_rect = {};     // Final position + size in absolute coords
    Rect content_rect = {};      // Inner rect (after padding + border)
    EdgeInsets computed_margin;  // Resolved margins
    EdgeInsets computed_padding; // Resolved padding

    // Dirty tracking
    bool layout_dirty = true;
};

/// Viewport info needed for resolving vw/vh/frac units
struct LayoutViewport {
    f32 width = 1280.0f;
    f32 height = 720.0f;
};

/// Runs the Yoga layout algorithm on a tree of LayoutNodes.
class LayoutEngine {
public:
    /// Compute layout for all nodes. The root node fills the viewport.
    void Compute(LayoutNode* nodes, u32 node_count, const LayoutViewport& viewport);
};

} // namespace ugui

#endif  // ULTRAGUI_LAYOUT_LAYOUT_H_
