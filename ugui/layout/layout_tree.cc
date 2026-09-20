#include <algorithm>
#include <ugui/layout/layout_tree.h>
#include <ugui/widgets/scroll_view.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

namespace ugui {

// A collapsed widget is `display: none`: the layout engine gives it no size and
// zeroes its whole subtree, so building nodes for what is under it is work with
// no result. Both walks below read the answer off the node's own style, the
// same copy apply_style() hands to Yoga, so they cannot disagree about where
// the tree was cut.
static bool SubtreeIsCollapsed(const LayoutNode& node) {
  return node.style.visibility == Visibility::kCollapsed;
}

// Walk the widget tree into the node array, refreshing only the entries whose
// widget asked for it.
//
// `store` holds the previous frame's array and is updated in place, so an
// entry whose widget is not layout-dirty keeps last frame's contents: that
// skips the part that costs anything, a whole Style copied out of the widget.
// The walk still visits every widget, because the tree links are rebuilt
// regardless and the test itself is one flag read.
//
// `reusable` is false on the first frame for a root; per entry, a widget key
// that does not match means the tree changed shape under us and that entry is
// repopulated whatever its flag says.
static void build_layout_nodes(WidgetRegistry& world, wid e, u32 parent_index,
                               LayoutEngine::NodeStore& store, bool reusable,
                               u32& cursor) {
  const u32 my_index = cursor++;
  if (my_index >= store.nodes.size()) {
    store.nodes.emplace_back();
    store.widget_keys.push_back(0);
  }

  const u32 key = world.Get<WidgetNode>(e)->id;
  const bool keep = reusable && store.widget_keys[my_index] == key;
  store.widget_keys[my_index] = key;

  Transform* t = world.Get<Transform>(e);
  LayoutNode& node = store.nodes[my_index];
  if (keep && !t->layout_dirty) {
    node.dirty = false;
    // Intrinsic size comes from the measure pass, which writes it straight
    // into the Transform without marking the widget dirty, and it also moves
    // when the ui scale does. It is two floats, so it is refreshed whatever
    // the flag says; the Style copy below is the part worth skipping.
    node.intrinsic_width = t->intrinsic_w;
    node.intrinsic_height = t->intrinsic_h;
  } else {
    PopulateLayoutNode(world, e, node);
    node.dirty = true;
    // Cleared here rather than after layout: a node whose rect does not move
    // is never visited by the apply walk, and would stay dirty forever.
    t->layout_dirty = false;
  }

  node.parent = parent_index;
  node.first_child = ~0u;
  node.next_sibling = ~0u;
  const bool collapsed = SubtreeIsCollapsed(node);
  const Vector<wid>& kids = world.Get<Hierarchy>(e)->children;
  node.child_count = collapsed ? 0u : static_cast<u32>(kids.size());

  // Link to parent.
  if (parent_index != ~0u) {
    auto& parent_node = store.nodes[parent_index];
    if (parent_node.first_child == ~0u) {
      parent_node.first_child = my_index;
    } else {
      u32 sib = parent_node.first_child;
      while (store.nodes[sib].next_sibling != ~0u)
        sib = store.nodes[sib].next_sibling;
      store.nodes[sib].next_sibling = my_index;
    }
  }

  if (collapsed) return;
  for (wid child : kids)
    build_layout_nodes(world, child, my_index, store, reusable, cursor);
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

  const bool collapsed = SubtreeIsCollapsed(node);

  ApplyLayoutResult(world, e, node);
  LayoutWidget(world, e, node.computed_rect, node.content_rect);

  ++node_index;
  if (collapsed) return;  // its children never got nodes; see build_layout_nodes
  for (wid child : world.Get<Hierarchy>(e)->children)
    apply_layout_results(world, child, node_index, nodes);
}

void ComputeWidgetLayout(wid root, const LayoutViewport& vp,
                         LayoutEngine& engine) {
  if (!root.valid()) return;
  WidgetRegistry& world = *WidgetRegistry::Active();

  LayoutEngine::NodeStore& store =
      engine.StoreFor(world.Get<WidgetNode>(root)->id);
  u32 cursor = 0;
  build_layout_nodes(world, root, ~0u, store, !store.nodes.empty(), cursor);
  store.nodes.resize(cursor);
  store.widget_keys.resize(cursor);

  engine.Compute(store.nodes.data(), static_cast<u32>(store.nodes.size()), vp);

  u32 idx = 0;
  apply_layout_results(world, root, idx, store.nodes);
}

}  // namespace ugui
