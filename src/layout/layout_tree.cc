#include <ultragui/layout/layout_tree.h>
#include <ultragui/widgets/scroll_view.h>
#include <ultragui/widgets/widget.h>

#include <algorithm>

namespace ugui {

static void build_layout_nodes(Widget* widget, u32 parent_index,
                               Vector<LayoutNode>& nodes) {
  u32 my_index = static_cast<u32>(nodes.size());

  LayoutNode node;
  widget->PopulateLayoutNode(node);
  node.parent = parent_index;
  node.first_child = ~0u;
  node.next_sibling = ~0u;
  node.child_count = widget->child_count();

  nodes.push_back(node);

  // Link to parent
  if (parent_index != ~0u) {
    auto& parent_node = nodes[parent_index];
    if (parent_node.first_child == ~0u) {
      parent_node.first_child = my_index;
    } else {
      u32 sib = parent_node.first_child;
      while (nodes[sib].next_sibling != ~0u) {
        sib = nodes[sib].next_sibling;
      }
      nodes[sib].next_sibling = my_index;
    }
  }

  // Recurse children
  for (u32 i = 0; i < widget->child_count(); ++i) {
    build_layout_nodes(widget->ChildAt(i), my_index, nodes);
  }
}

/// Walk up from `widget` to find the nearest ScrollView ancestor.
static ScrollView* FindScrollParent(Widget* widget) {
  Widget* p = widget->parent();
  while (p) {
    if (auto* sv = dynamic_cast<ScrollView*>(p)) return sv;
    p = p->parent();
  }
  return nullptr;
}

static void apply_layout_results(Widget* widget, u32& node_index,
                                 Vector<LayoutNode>& nodes) {
  auto& node = nodes[node_index];

  // Sticky positioning: clamp the widget's y so it stays pinned to the
  // top of the scroll parent's visible region when scrolled past.
  if (node.style.position == Position::kSticky) {
    if (auto* sv = FindScrollParent(widget)) {
      Rect visible = sv->rect();
      Vec2 offset = sv->scroll_offset();
      f32 sticky_min_y = visible.y + offset.y;
      if (node.computed_rect.y < sticky_min_y) {
        node.computed_rect.y = sticky_min_y;
        // Clamp to bottom of scroll parent so it doesn't overflow
        f32 max_y = visible.y + visible.h - node.computed_rect.h;
        node.computed_rect.y = std::min(node.computed_rect.y, max_y);
      }
    }
  }

  widget->ApplyLayoutResult(node);
  widget->OnLayout(node.computed_rect, node.content_rect);

  ++node_index;
  for (u32 i = 0; i < widget->child_count(); ++i) {
    apply_layout_results(widget->ChildAt(i), node_index, nodes);
  }
}

void ComputeWidgetLayout(Widget* root, const LayoutViewport& vp,
                         LayoutEngine& engine, Vector<LayoutNode>& scratch) {
  if (!root) return;

  scratch.clear();
  build_layout_nodes(root, ~0u, scratch);

  engine.Compute(scratch.data(), static_cast<u32>(scratch.size()), vp);

  u32 idx = 0;
  apply_layout_results(root, idx, scratch);
}

}  // namespace ugui
