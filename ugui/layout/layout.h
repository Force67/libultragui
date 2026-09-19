#ifndef ULTRAGUI_LAYOUT_LAYOUT_H_
#define ULTRAGUI_LAYOUT_LAYOUT_H_

#include <ugui/core/rect.h>
#include <ugui/core/types.h>
#include <ugui/style/style.h>

namespace ugui {

/// A node in the layout tree. Each node has a Style and produces a computed
/// Rect. This is the input/output structure for the layout algorithm.
struct LayoutNode {
  // Input
  Style style;
  u32 id = 0;  // Widget ID for mapping back

  // Intrinsic content size (e.g. text bounding box, image size)
  f32 intrinsic_width = 0.0f;
  f32 intrinsic_height = 0.0f;

  // Tree structure (indices into the LayoutContext's node array)
  u32 parent = ~0u;
  u32 first_child = ~0u;
  u32 next_sibling = ~0u;
  u32 child_count = 0;

  // Output (computed by layout)
  Rect computed_rect = {};      // Final position + size in absolute coords
  Rect content_rect = {};       // Inner rect (after padding + border)
  EdgeInsets computed_margin;   // Resolved margins
  EdgeInsets computed_padding;  // Resolved padding

  // Dirty tracking
  bool layout_dirty = true;
};

/// Viewport info needed for resolving vw/vh/frac units
struct LayoutViewport {
  f32 width = 1280.0f;
  f32 height = 720.0f;
  f32 scale = 1.0f;  ///< Viewport scale factor applied to all kPx values
};

/// Runs the Yoga layout algorithm on a tree of LayoutNodes.
///
/// The Yoga tree is retained between calls. Yoga caches each node's measured
/// layout and re-solves only the subtrees that changed, which it cannot do for
/// a tree that is thrown away and rebuilt every frame. Compute() notices when
/// the shape it is handed no longer matches the tree it holds and rebuilds.
class LayoutEngine {
 public:
  LayoutEngine() = default;
  ~LayoutEngine();
  LayoutEngine(const LayoutEngine&) = delete;
  LayoutEngine& operator=(const LayoutEngine&) = delete;

  /// Compute layout for all nodes. The root node fills the viewport.
  void Compute(LayoutNode* nodes, u32 node_count,
               const LayoutViewport& viewport);

  /// Drop the retained tree. Only needed to release the memory early; a
  /// changed tree is detected and rebuilt by Compute() on its own.
  void Reset();

 private:
  struct Retained;
  Retained* retained_ = nullptr;
};

}  // namespace ugui

#endif  // ULTRAGUI_LAYOUT_LAYOUT_H_
