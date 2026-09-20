#include <ugui/layout/layout.h>

#include <algorithm>
#include <unordered_map>
#include <vector>
#include <yoga/Yoga.h>

namespace ugui {

// ---------------------------------------------------------------------------
// Helpers: map ugui Style -> Yoga node style
// ---------------------------------------------------------------------------

static void set_yoga_length_w(YGNodeRef yg, const Length& len, f32 vw, f32 vh,
                              f32 scale, void (*set_px)(YGNodeRef, float),
                              void (*set_pct)(YGNodeRef, float),
                              void (*set_auto)(YGNodeRef) = nullptr) {
  switch (len.unit) {
    case Length::Unit::kPx:
      set_px(yg, len.value * scale);
      break;
    case Length::Unit::kPercent:
      set_pct(yg, len.value);
      break;
    case Length::Unit::kVw:
      set_px(yg, len.value * 0.01f * vw);
      break;
    case Length::Unit::kVh:
      set_px(yg, len.value * 0.01f * vh);
      break;
    case Length::Unit::kFrac:
      set_px(yg, len.value * vw);  // width axis
      break;
    case Length::Unit::kAuto:
      if (set_auto) set_auto(yg);
      break;
  }
}

static void set_yoga_length_h(YGNodeRef yg, const Length& len, f32 vw, f32 vh,
                              f32 scale, void (*set_px)(YGNodeRef, float),
                              void (*set_pct)(YGNodeRef, float),
                              void (*set_auto)(YGNodeRef) = nullptr) {
  switch (len.unit) {
    case Length::Unit::kPx:
      set_px(yg, len.value * scale);
      break;
    case Length::Unit::kPercent:
      set_pct(yg, len.value);
      break;
    case Length::Unit::kVw:
      set_px(yg, len.value * 0.01f * vw);
      break;
    case Length::Unit::kVh:
      set_px(yg, len.value * 0.01f * vh);
      break;
    case Length::Unit::kFrac:
      set_px(yg, len.value * vh);  // height axis
      break;
    case Length::Unit::kAuto:
      if (set_auto) set_auto(yg);
      break;
  }
}

static YGFlexDirection map_flex_direction(FlexDirection d) {
  switch (d) {
    case FlexDirection::kRow:
      return YGFlexDirectionRow;
    case FlexDirection::kColumn:
      return YGFlexDirectionColumn;
    case FlexDirection::kRowReverse:
      return YGFlexDirectionRowReverse;
    case FlexDirection::kColumnReverse:
      return YGFlexDirectionColumnReverse;
  }
  return YGFlexDirectionRow;
}

static YGJustify map_justify(JustifyContent j) {
  switch (j) {
    case JustifyContent::kStart:
      return YGJustifyFlexStart;
    case JustifyContent::kEnd:
      return YGJustifyFlexEnd;
    case JustifyContent::kCenter:
      return YGJustifyCenter;
    case JustifyContent::kSpaceBetween:
      return YGJustifySpaceBetween;
    case JustifyContent::kSpaceAround:
      return YGJustifySpaceAround;
    case JustifyContent::kSpaceEvenly:
      return YGJustifySpaceEvenly;
  }
  return YGJustifyFlexStart;
}

static YGAlign map_align_items(AlignItems a) {
  switch (a) {
    case AlignItems::kStart:
      return YGAlignFlexStart;
    case AlignItems::kEnd:
      return YGAlignFlexEnd;
    case AlignItems::kCenter:
      return YGAlignCenter;
    case AlignItems::kStretch:
      return YGAlignStretch;
    case AlignItems::kBaseline:
      return YGAlignBaseline;
  }
  return YGAlignStretch;
}

static YGAlign map_align_self(AlignSelf a) {
  switch (a) {
    case AlignSelf::kAuto:
      return YGAlignAuto;
    case AlignSelf::kStart:
      return YGAlignFlexStart;
    case AlignSelf::kEnd:
      return YGAlignFlexEnd;
    case AlignSelf::kCenter:
      return YGAlignCenter;
    case AlignSelf::kStretch:
      return YGAlignStretch;
  }
  return YGAlignAuto;
}

static YGAlign map_align_content(AlignContent a) {
  switch (a) {
    case AlignContent::kStart:
      return YGAlignFlexStart;
    case AlignContent::kEnd:
      return YGAlignFlexEnd;
    case AlignContent::kCenter:
      return YGAlignCenter;
    case AlignContent::kStretch:
      return YGAlignStretch;
    case AlignContent::kSpaceBetween:
      return YGAlignSpaceBetween;
    case AlignContent::kSpaceAround:
      return YGAlignSpaceAround;
    case AlignContent::kSpaceEvenly:
      return YGAlignSpaceEvenly;
  }
  return YGAlignFlexStart;
}

static YGWrap map_wrap(FlexWrap w) {
  switch (w) {
    case FlexWrap::kNoWrap:
      return YGWrapNoWrap;
    case FlexWrap::kWrap:
      return YGWrapWrap;
    case FlexWrap::kWrapReverse:
      return YGWrapWrapReverse;
  }
  return YGWrapNoWrap;
}

static YGOverflow map_overflow(Overflow o) {
  switch (o) {
    case Overflow::kVisible:
      return YGOverflowVisible;
    case Overflow::kHidden:
      return YGOverflowHidden;
    case Overflow::kScroll:
      return YGOverflowScroll;
  }
  return YGOverflowVisible;
}

static YGPositionType map_position(Position p) {
  switch (p) {
    case Position::kRelative:
      return YGPositionTypeRelative;
    case Position::kAbsolute:
      return YGPositionTypeAbsolute;
    case Position::kSticky:
      // Sticky participates in normal flow like relative; the clamping to
      // the scroll parent's visible area is handled after layout in
      // layout_tree.cc's apply_layout_results().
      return YGPositionTypeRelative;
  }
  return YGPositionTypeRelative;
}

static YGSize yoga_measure_func(YGNodeConstRef node, float /*width*/,
                                YGMeasureMode /*widthMode*/, float /*height*/,
                                YGMeasureMode /*heightMode*/) {
  auto* ln = static_cast<LayoutNode*>(YGNodeGetContext(node));
  return {ln->intrinsic_width, ln->intrinsic_height};
}

// Write a Style onto a Yoga node.
//
// Every property is written on every pass, including the ones the style does
// not constrain (cleared to YGUndefined instead of being left alone). The nodes
// are retained between frames, so a property that is only written under a
// condition keeps whatever the last frame that met the condition put there: a
// panel that stops being collapsed would stay `display: none` forever. Yoga's
// setters compare before they mark a node dirty, so writing the same value back
// every frame costs a comparison and leaves the layout cache intact.
static void apply_style(YGNodeRef yg, const Style& s,
                        const LayoutViewport& vp) {
  f32 vw = vp.width;
  f32 vh = vp.height;
  f32 sc = vp.scale;

  YGNodeStyleSetFlexDirection(yg, map_flex_direction(s.flex_direction));
  YGNodeStyleSetJustifyContent(yg, map_justify(s.justify_content));
  YGNodeStyleSetAlignItems(yg, map_align_items(s.align_items));
  YGNodeStyleSetAlignSelf(yg, map_align_self(s.align_self));
  YGNodeStyleSetAlignContent(yg, map_align_content(s.align_content));
  YGNodeStyleSetFlexWrap(yg, map_wrap(s.flex_wrap));
  YGNodeStyleSetOverflow(yg, map_overflow(s.overflow));
  YGNodeStyleSetPositionType(yg, map_position(s.position));

  YGNodeStyleSetDisplay(yg, s.visibility == Visibility::kCollapsed
                                ? YGDisplayNone
                                : YGDisplayFlex);

  // Flex
  YGNodeStyleSetFlexGrow(yg, s.flex_grow);
  YGNodeStyleSetFlexShrink(yg, s.flex_shrink);
  switch (s.flex_basis.unit) {
    case Length::Unit::kAuto:
      YGNodeStyleSetFlexBasisAuto(yg);
      break;
    case Length::Unit::kPercent:
      YGNodeStyleSetFlexBasisPercent(yg, s.flex_basis.value);
      break;
    default:  // px/vw/vh/fr resolve to pixels along the main axis
      YGNodeStyleSetFlexBasis(
          yg, s.flex_basis.Resolve(0, vw, vh,
                                   s.flex_direction == FlexDirection::kColumn ||
                                       s.flex_direction ==
                                           FlexDirection::kColumnReverse) *
                  sc);
      break;
  }

  // Sizing
  set_yoga_length_w(yg, s.width, vw, vh, sc, YGNodeStyleSetWidth,
                    YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto);
  set_yoga_length_h(yg, s.height, vw, vh, sc, YGNodeStyleSetHeight,
                    YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto);

  // Min/max: the defaults (min 0, max 1e6) mean "unconstrained", which is
  // YGUndefined rather than a number Yoga would have to honour.
  if (s.min_width.value > 0.0f)
    set_yoga_length_w(yg, s.min_width, vw, vh, sc, YGNodeStyleSetMinWidth,
                      YGNodeStyleSetMinWidthPercent);
  else
    YGNodeStyleSetMinWidth(yg, YGUndefined);
  if (s.min_height.value > 0.0f)
    set_yoga_length_h(yg, s.min_height, vw, vh, sc, YGNodeStyleSetMinHeight,
                      YGNodeStyleSetMinHeightPercent);
  else
    YGNodeStyleSetMinHeight(yg, YGUndefined);
  if (s.max_width.value < 1e5f)
    set_yoga_length_w(yg, s.max_width, vw, vh, sc, YGNodeStyleSetMaxWidth,
                      YGNodeStyleSetMaxWidthPercent);
  else
    YGNodeStyleSetMaxWidth(yg, YGUndefined);
  if (s.max_height.value < 1e5f)
    set_yoga_length_h(yg, s.max_height, vw, vh, sc, YGNodeStyleSetMaxHeight,
                      YGNodeStyleSetMaxHeightPercent);
  else
    YGNodeStyleSetMaxHeight(yg, YGUndefined);

  // Margin (pixel values: scale them)
  YGNodeStyleSetMargin(yg, YGEdgeTop, s.margin.top * sc);
  YGNodeStyleSetMargin(yg, YGEdgeRight, s.margin.right * sc);
  YGNodeStyleSetMargin(yg, YGEdgeBottom, s.margin.bottom * sc);
  YGNodeStyleSetMargin(yg, YGEdgeLeft, s.margin.left * sc);

  // Padding (pixel values: scale them)
  YGNodeStyleSetPadding(yg, YGEdgeTop, s.padding.top * sc);
  YGNodeStyleSetPadding(yg, YGEdgeRight, s.padding.right * sc);
  YGNodeStyleSetPadding(yg, YGEdgeBottom, s.padding.bottom * sc);
  YGNodeStyleSetPadding(yg, YGEdgeLeft, s.padding.left * sc);

  // Gap: uniform, with optional per-axis overrides. A negative per-axis value
  // means "defer to the uniform gap", which is what clearing it to undefined
  // tells Yoga.
  YGNodeStyleSetGap(yg, YGGutterAll, s.gap > 0.0f ? s.gap * sc : YGUndefined);
  YGNodeStyleSetGap(yg, YGGutterRow,
                    s.row_gap >= 0.0f ? s.row_gap * sc : YGUndefined);
  YGNodeStyleSetGap(yg, YGGutterColumn,
                    s.column_gap >= 0.0f ? s.column_gap * sc : YGUndefined);

  // Aspect ratio
  YGNodeStyleSetAspectRatio(
      yg, s.aspect_ratio > 0.0f ? s.aspect_ratio : YGUndefined);

  // Position offsets (resolve, then scale the px result)
  if (!s.top.IsAuto())
    YGNodeStyleSetPosition(yg, YGEdgeTop, s.top.Resolve(0, vw, vh, true) * sc);
  else
    YGNodeStyleSetPosition(yg, YGEdgeTop, YGUndefined);
  if (!s.right_offset.IsAuto())
    YGNodeStyleSetPosition(yg, YGEdgeRight,
                           s.right_offset.Resolve(0, vw, vh, false) * sc);
  else
    YGNodeStyleSetPosition(yg, YGEdgeRight, YGUndefined);
  if (!s.bottom.IsAuto())
    YGNodeStyleSetPosition(yg, YGEdgeBottom,
                           s.bottom.Resolve(0, vw, vh, true) * sc);
  else
    YGNodeStyleSetPosition(yg, YGEdgeBottom, YGUndefined);
  if (!s.left_offset.IsAuto())
    YGNodeStyleSetPosition(yg, YGEdgeLeft,
                           s.left_offset.Resolve(0, vw, vh, false) * sc);
  else
    YGNodeStyleSetPosition(yg, YGEdgeLeft, YGUndefined);
}

// ---------------------------------------------------------------------------
// Readback: Yoga results -> LayoutNode
// ---------------------------------------------------------------------------

static void readback_results(YGNodeRef yg, LayoutNode* nodes, u32 node_index,
                             f32 offset_x, f32 offset_y) {
  auto& node = nodes[node_index];

  f32 x = YGNodeLayoutGetLeft(yg) + offset_x;
  f32 y = YGNodeLayoutGetTop(yg) + offset_y;
  f32 w = YGNodeLayoutGetWidth(yg);
  f32 h = YGNodeLayoutGetHeight(yg);

  node.computed_rect = {x, y, w, h};

  node.computed_padding = {
      YGNodeLayoutGetPadding(yg, YGEdgeTop),
      YGNodeLayoutGetPadding(yg, YGEdgeRight),
      YGNodeLayoutGetPadding(yg, YGEdgeBottom),
      YGNodeLayoutGetPadding(yg, YGEdgeLeft),
  };

  node.computed_margin = {
      YGNodeLayoutGetMargin(yg, YGEdgeTop),
      YGNodeLayoutGetMargin(yg, YGEdgeRight),
      YGNodeLayoutGetMargin(yg, YGEdgeBottom),
      YGNodeLayoutGetMargin(yg, YGEdgeLeft),
  };

  node.content_rect = {
      x + node.computed_padding.left,
      y + node.computed_padding.top,
      std::max(w - node.computed_padding.horizontal(), 0.0f),
      std::max(h - node.computed_padding.vertical(), 0.0f),
  };

  node.layout_dirty = false;

  // Recurse children: Yoga positions are relative to parent's border box
  // (padding is already accounted for in the child's position), so pass
  // only the parent's absolute origin, not origin + padding.
  u32 child_idx = node.first_child;
  u32 yg_child_i = 0;
  while (child_idx != ~0u) {
    YGNodeRef child_yg = YGNodeGetChild(yg, yg_child_i);
    readback_results(child_yg, nodes, child_idx, x, y);
    child_idx = nodes[child_idx].next_sibling;
    ++yg_child_i;
  }
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

// What the retained tree was built for. Compute() rebuilds when any of this
// stops matching: the Yoga nodes are addressed by index, so a tree of a
// different shape would silently pair widgets with someone else's node.
struct RetainedShape {
  u32 id = 0;
  u32 parent = ~0u;
  u32 first_child = ~0u;
  u32 next_sibling = ~0u;
  bool measured = false;

  bool operator==(const RetainedShape&) const = default;
};

static RetainedShape ShapeOf(const LayoutNode& n) {
  return RetainedShape{
      n.id, n.parent, n.first_child, n.next_sibling,
      // A leaf with intrinsic content measures itself through the context.
      n.first_child == ~0u &&
          (n.intrinsic_width > 0.0f || n.intrinsic_height > 0.0f)};
}

// One retained Yoga tree.
struct RetainedTree {
  std::vector<YGNodeRef> nodes;
  std::vector<RetainedShape> shape;
  // The style last written to each node. apply_style is a pure function of
  // (style, viewport), so a node whose style has not moved since the last pass
  // can skip it: seventy-odd setter calls that would each compare and find
  // nothing changed.
  std::vector<Style> applied;
  LayoutViewport applied_viewport{0.0f, 0.0f, 0.0f};
  // Last intrinsic size handed to each measured node. Yoga caches what a
  // measure function returned and has no way to know the answer changed, so a
  // node whose text was re-shaped has to be marked dirty by hand.
  std::vector<Vec2> intrinsic;
};

// One engine serves several trees - the main root, each overlay, each
// offscreen pass - and they interleave within a frame. Keyed by root widget so
// they do not evict one another; a single tree would rebuild on every call.
struct LayoutEngine::Retained {
  YGConfigRef config = nullptr;
  std::unordered_map<u32, RetainedTree> trees;
  std::unordered_map<u32, LayoutEngine::NodeStore> stores;

  ~Retained() {
    for (auto& [root_id, tree] : trees)
      if (!tree.nodes.empty()) YGNodeFreeRecursive(tree.nodes[0]);
    if (config) YGConfigFree(config);
  }
};

LayoutEngine::~LayoutEngine() {
  delete retained_;
}

void LayoutEngine::Reset() {
  delete retained_;
  retained_ = nullptr;
}

LayoutEngine::NodeStore& LayoutEngine::StoreFor(u32 root_id) {
  if (!retained_) retained_ = new Retained();
  return retained_->stores[root_id];
}

void LayoutEngine::Compute(LayoutNode* nodes, u32 node_count,
                           const LayoutViewport& viewport) {
  if (node_count == 0) return;

  if (!retained_) retained_ = new Retained();
  if (!retained_->config) retained_->config = YGConfigNew();
  RetainedTree& r = retained_->trees[nodes[0].id];

  bool same_tree = r.shape.size() == node_count;
  if (same_tree) {
    for (u32 i = 0; i < node_count; ++i) {
      if (!(r.shape[i] == ShapeOf(nodes[i]))) {
        same_tree = false;
        break;
      }
    }
  }

  if (!same_tree) {
    if (!r.nodes.empty()) YGNodeFreeRecursive(r.nodes[0]);
    r.nodes.assign(node_count, nullptr);
    r.shape.resize(node_count);
    r.intrinsic.assign(node_count, Vec2{});
    r.applied.assign(node_count, Style{});
    r.applied_viewport = {0.0f, 0.0f, 0.0f};  // force the first write
    for (u32 i = 0; i < node_count; ++i) {
      r.nodes[i] = YGNodeNewWithConfig(retained_->config);
      r.shape[i] = ShapeOf(nodes[i]);
      if (r.shape[i].measured)
        YGNodeSetMeasureFunc(r.nodes[i], yoga_measure_func);
    }
    for (u32 i = 0; i < node_count; ++i) {
      u32 child_idx = nodes[i].first_child;
      u32 insert_pos = 0;
      while (child_idx != ~0u) {
        YGNodeInsertChild(r.nodes[i], r.nodes[child_idx], insert_pos);
        child_idx = nodes[child_idx].next_sibling;
        ++insert_pos;
      }
    }
  }

  // A resize changes what every vw/vh/scaled px resolves to, so nothing can be
  // skipped on the pass that first sees a new viewport.
  const bool viewport_moved = !(r.applied_viewport.width == viewport.width &&
                                r.applied_viewport.height == viewport.height &&
                                r.applied_viewport.scale == viewport.scale);
  r.applied_viewport = viewport;

  for (u32 i = 0; i < node_count; ++i) {
    // The LayoutNode array is the caller's scratch and moves between frames,
    // so the context has to be re-pointed even when the tree was reused.
    YGNodeSetContext(r.nodes[i], &nodes[i]);
    // A node the caller did not refresh still holds the style that was applied
    // last frame, so there is nothing to compare and nothing to write.
    // A resize changes what every length resolves to, so it re-applies
    // everything; otherwise only a node the caller refreshed can differ.
    if (viewport_moved ||
        (nodes[i].dirty && !(r.applied[i] == nodes[i].style))) {
      apply_style(r.nodes[i], nodes[i].style, viewport);
      r.applied[i] = nodes[i].style;
    }

    if (r.shape[i].measured) {
      const Vec2 intrinsic{nodes[i].intrinsic_width, nodes[i].intrinsic_height};
      if (intrinsic.x != r.intrinsic[i].x || intrinsic.y != r.intrinsic[i].y) {
        r.intrinsic[i] = intrinsic;
        YGNodeMarkDirty(r.nodes[i]);
      }
    }
  }

  // Clean nodes whose available size is unchanged come straight out of Yoga's
  // layout cache, so a frame in which nothing moved costs a walk, not a solve.
  YGNodeCalculateLayout(r.nodes[0], viewport.width, viewport.height,
                        YGDirectionLTR);

  // Read back results into LayoutNodes (converting parent-relative -> absolute)
  readback_results(r.nodes[0], nodes, 0, 0.0f, 0.0f);
}

}  // namespace ugui
