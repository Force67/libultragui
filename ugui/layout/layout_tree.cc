#include <algorithm>
#include <ugui/layout/layout_tree.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

namespace ugui {

static void build_layout_nodes(WidgetRegistry& world, wid e, u32 parent_index,
                               Vector<LayoutNode>& nodes) {
  u32 my_index = static_cast<u32>(nodes.size());

  LayoutNode node;
  PopulateLayoutNode(world, e, node);
  node.parent = parent_index;
  node.first_child = ~0u;
  node.next_sibling = ~0u;
  const Vector<wid>& kids = world.Get<Hierarchy>(e)->children;
  node.child_count = static_cast<u32>(kids.size());

  nodes.push_back(node);

  // Link to parent.
  if (parent_index != ~0u) {
    auto& parent_node = nodes[parent_index];
    if (parent_node.first_child == ~0u) {
      parent_node.first_child = my_index;
    } else {
      u32 sib = parent_node.first_child;
      while (nodes[sib].next_sibling != ~0u) sib = nodes[sib].next_sibling;
      nodes[sib].next_sibling = my_index;
    }
  }

  for (wid child : kids) build_layout_nodes(world, child, my_index, nodes);
}

/// Walk up from `e` to find the nearest scroll-view ancestor.
static wid FindScrollParent(WidgetRegistry& world, wid e) {
  wid p = world.Get<Hierarchy>(e)->parent;
  while (p.valid()) {
    WidgetNode* n = world.Get<WidgetNode>(p);
    if (n && n->kind == WidgetKind::kScrollView) return p;
    Hierarchy* h = world.Get<Hierarchy>(p);
    p = h ? h->parent : kNullWidget;
  }
  return kNullWidget;
}

static void apply_layout_results(WidgetRegistry& world, wid e, u32& node_index,
                                 Vector<LayoutNode>& nodes) {
  auto& node = nodes[node_index];

  // Sticky positioning: clamp y so it stays pinned to the top of the scroll
  // parent's visible region when scrolled past.
  if (node.style.position == Position::kSticky) {
    if (wid sv = FindScrollParent(world, e); sv.valid()) {
      Rect visible = world.Get<Transform>(sv)->rect;
      Vec2 offset = ScrollOffset(world, sv);
      f32 sticky_min_y = visible.y + offset.y;
      if (node.computed_rect.y < sticky_min_y) {
        node.computed_rect.y = sticky_min_y;
        f32 max_y = visible.y + visible.h - node.computed_rect.h;
        node.computed_rect.y = std::min(node.computed_rect.y, max_y);
      }
    }
  }

  ApplyLayoutResult(world, e, node);
  LayoutWidget(world, e, node.computed_rect, node.content_rect);

  ++node_index;
  for (wid child : world.Get<Hierarchy>(e)->children)
    apply_layout_results(world, child, node_index, nodes);
}

void ComputeWidgetLayout(wid root, const LayoutViewport& vp,
                         LayoutEngine& engine, Vector<LayoutNode>& scratch) {
  if (!root.valid()) return;
  WidgetRegistry& world = *WidgetRegistry::Active();

  scratch.clear();
  build_layout_nodes(world, root, ~0u, scratch);

  engine.Compute(scratch.data(), static_cast<u32>(scratch.size()), vp);

  u32 idx = 0;
  apply_layout_results(world, root, idx, scratch);
}

}  // namespace ugui
