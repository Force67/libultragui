#include <ultragui/layout/layout.h>

#include <yoga/Yoga.h>

#include <algorithm>
#include <vector>

namespace ugui {

// ---------------------------------------------------------------------------
// Helpers: map ugui Style -> Yoga node style
// ---------------------------------------------------------------------------

static void set_yoga_length_w(YGNodeRef yg, const Length& len, f32 vw, f32 vh,
                              void (*set_px)(YGNodeRef, float),
                              void (*set_pct)(YGNodeRef, float),
                              void (*set_auto)(YGNodeRef) = nullptr) {
    switch (len.unit) {
    case Length::Unit::Px:
        set_px(yg, len.value);
        break;
    case Length::Unit::Percent:
        set_pct(yg, len.value);
        break;
    case Length::Unit::Vw:
        set_px(yg, len.value * 0.01f * vw);
        break;
    case Length::Unit::Vh:
        set_px(yg, len.value * 0.01f * vh);
        break;
    case Length::Unit::Frac:
        set_px(yg, len.value * vw); // width axis
        break;
    case Length::Unit::Auto:
        if (set_auto)
            set_auto(yg);
        break;
    }
}

static void set_yoga_length_h(YGNodeRef yg, const Length& len, f32 vw, f32 vh,
                              void (*set_px)(YGNodeRef, float),
                              void (*set_pct)(YGNodeRef, float),
                              void (*set_auto)(YGNodeRef) = nullptr) {
    switch (len.unit) {
    case Length::Unit::Px:
        set_px(yg, len.value);
        break;
    case Length::Unit::Percent:
        set_pct(yg, len.value);
        break;
    case Length::Unit::Vw:
        set_px(yg, len.value * 0.01f * vw);
        break;
    case Length::Unit::Vh:
        set_px(yg, len.value * 0.01f * vh);
        break;
    case Length::Unit::Frac:
        set_px(yg, len.value * vh); // height axis
        break;
    case Length::Unit::Auto:
        if (set_auto)
            set_auto(yg);
        break;
    }
}

static YGFlexDirection map_flex_direction(FlexDirection d) {
    switch (d) {
    case FlexDirection::Row:
        return YGFlexDirectionRow;
    case FlexDirection::Column:
        return YGFlexDirectionColumn;
    case FlexDirection::RowReverse:
        return YGFlexDirectionRowReverse;
    case FlexDirection::ColumnReverse:
        return YGFlexDirectionColumnReverse;
    }
    return YGFlexDirectionRow;
}

static YGJustify map_justify(JustifyContent j) {
    switch (j) {
    case JustifyContent::Start:
        return YGJustifyFlexStart;
    case JustifyContent::End:
        return YGJustifyFlexEnd;
    case JustifyContent::Center:
        return YGJustifyCenter;
    case JustifyContent::SpaceBetween:
        return YGJustifySpaceBetween;
    case JustifyContent::SpaceAround:
        return YGJustifySpaceAround;
    case JustifyContent::SpaceEvenly:
        return YGJustifySpaceEvenly;
    }
    return YGJustifyFlexStart;
}

static YGAlign map_align_items(AlignItems a) {
    switch (a) {
    case AlignItems::Start:
        return YGAlignFlexStart;
    case AlignItems::End:
        return YGAlignFlexEnd;
    case AlignItems::Center:
        return YGAlignCenter;
    case AlignItems::Stretch:
        return YGAlignStretch;
    case AlignItems::Baseline:
        return YGAlignBaseline;
    }
    return YGAlignStretch;
}

static YGAlign map_align_self(AlignSelf a) {
    switch (a) {
    case AlignSelf::Auto:
        return YGAlignAuto;
    case AlignSelf::Start:
        return YGAlignFlexStart;
    case AlignSelf::End:
        return YGAlignFlexEnd;
    case AlignSelf::Center:
        return YGAlignCenter;
    case AlignSelf::Stretch:
        return YGAlignStretch;
    }
    return YGAlignAuto;
}

static YGWrap map_wrap(FlexWrap w) {
    switch (w) {
    case FlexWrap::NoWrap:
        return YGWrapNoWrap;
    case FlexWrap::Wrap:
        return YGWrapWrap;
    case FlexWrap::WrapReverse:
        return YGWrapWrapReverse;
    }
    return YGWrapNoWrap;
}

static YGOverflow map_overflow(Overflow o) {
    switch (o) {
    case Overflow::Visible:
        return YGOverflowVisible;
    case Overflow::Hidden:
        return YGOverflowHidden;
    case Overflow::Scroll:
        return YGOverflowScroll;
    }
    return YGOverflowVisible;
}

static YGPositionType map_position(Position p) {
    switch (p) {
    case Position::Relative:
        return YGPositionTypeRelative;
    case Position::Absolute:
        return YGPositionTypeAbsolute;
    }
    return YGPositionTypeRelative;
}

static YGSize yoga_measure_func(YGNodeConstRef node, float /*width*/,
                                YGMeasureMode /*widthMode*/, float /*height*/,
                                YGMeasureMode /*heightMode*/) {
    auto* ln = static_cast<LayoutNode*>(YGNodeGetContext(node));
    return {ln->intrinsic_width, ln->intrinsic_height};
}

static void apply_style(YGNodeRef yg, const Style& s, const LayoutViewport& vp) {
    f32 vw = vp.width;
    f32 vh = vp.height;

    YGNodeStyleSetFlexDirection(yg, map_flex_direction(s.flex_direction));
    YGNodeStyleSetJustifyContent(yg, map_justify(s.justify_content));
    YGNodeStyleSetAlignItems(yg, map_align_items(s.align_items));
    YGNodeStyleSetAlignSelf(yg, map_align_self(s.align_self));
    YGNodeStyleSetFlexWrap(yg, map_wrap(s.flex_wrap));
    YGNodeStyleSetOverflow(yg, map_overflow(s.overflow));
    YGNodeStyleSetPositionType(yg, map_position(s.position));

    if (s.visibility == Visibility::Collapsed)
        YGNodeStyleSetDisplay(yg, YGDisplayNone);

    // Flex
    YGNodeStyleSetFlexGrow(yg, s.flex_grow);
    YGNodeStyleSetFlexShrink(yg, s.flex_shrink);

    // Sizing
    set_yoga_length_w(yg, s.width, vw, vh, YGNodeStyleSetWidth, YGNodeStyleSetWidthPercent,
                      YGNodeStyleSetWidthAuto);
    set_yoga_length_h(yg, s.height, vw, vh, YGNodeStyleSetHeight, YGNodeStyleSetHeightPercent,
                      YGNodeStyleSetHeightAuto);

    // Min/max - skip the default "unbounded" sentinel (1e6)
    if (s.min_width.value > 0.0f)
        set_yoga_length_w(yg, s.min_width, vw, vh, YGNodeStyleSetMinWidth,
                          YGNodeStyleSetMinWidthPercent);
    if (s.min_height.value > 0.0f)
        set_yoga_length_h(yg, s.min_height, vw, vh, YGNodeStyleSetMinHeight,
                          YGNodeStyleSetMinHeightPercent);
    if (s.max_width.value < 1e5f)
        set_yoga_length_w(yg, s.max_width, vw, vh, YGNodeStyleSetMaxWidth,
                          YGNodeStyleSetMaxWidthPercent);
    if (s.max_height.value < 1e5f)
        set_yoga_length_h(yg, s.max_height, vw, vh, YGNodeStyleSetMaxHeight,
                          YGNodeStyleSetMaxHeightPercent);

    // Margin
    YGNodeStyleSetMargin(yg, YGEdgeTop, s.margin.top);
    YGNodeStyleSetMargin(yg, YGEdgeRight, s.margin.right);
    YGNodeStyleSetMargin(yg, YGEdgeBottom, s.margin.bottom);
    YGNodeStyleSetMargin(yg, YGEdgeLeft, s.margin.left);

    // Padding
    YGNodeStyleSetPadding(yg, YGEdgeTop, s.padding.top);
    YGNodeStyleSetPadding(yg, YGEdgeRight, s.padding.right);
    YGNodeStyleSetPadding(yg, YGEdgeBottom, s.padding.bottom);
    YGNodeStyleSetPadding(yg, YGEdgeLeft, s.padding.left);

    // Gap
    if (s.gap > 0.0f)
        YGNodeStyleSetGap(yg, YGGutterAll, s.gap);

    // Position offsets
    if (!s.top.is_auto())
        YGNodeStyleSetPosition(yg, YGEdgeTop, s.top.resolve(0, vw, vh, true));
    if (!s.right_offset.is_auto())
        YGNodeStyleSetPosition(yg, YGEdgeRight, s.right_offset.resolve(0, vw, vh, false));
    if (!s.bottom.is_auto())
        YGNodeStyleSetPosition(yg, YGEdgeBottom, s.bottom.resolve(0, vw, vh, true));
    if (!s.left_offset.is_auto())
        YGNodeStyleSetPosition(yg, YGEdgeLeft, s.left_offset.resolve(0, vw, vh, false));
}

// ---------------------------------------------------------------------------
// Readback: Yoga results -> LayoutNode
// ---------------------------------------------------------------------------

static void readback_results(YGNodeRef yg, LayoutNode* nodes, u32 node_index, f32 offset_x,
                             f32 offset_y) {
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

    // Recurse children - Yoga positions are relative to parent's border box
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

void LayoutEngine::compute(LayoutNode* nodes, u32 node_count, const LayoutViewport& viewport) {
    if (node_count == 0)
        return;

    YGConfigRef config = YGConfigNew();

    // Create Yoga nodes
    std::vector<YGNodeRef> yg_nodes(node_count);
    for (u32 i = 0; i < node_count; ++i) {
        yg_nodes[i] = YGNodeNewWithConfig(config);
        YGNodeSetContext(yg_nodes[i], &nodes[i]);
        apply_style(yg_nodes[i], nodes[i].style, viewport);

        // Leaf nodes with intrinsic content get a measure function
        if (nodes[i].first_child == ~0u &&
            (nodes[i].intrinsic_width > 0.0f || nodes[i].intrinsic_height > 0.0f)) {
            YGNodeSetMeasureFunc(yg_nodes[i], yoga_measure_func);
        }
    }

    // Wire parent-child relationships
    for (u32 i = 0; i < node_count; ++i) {
        u32 child_idx = nodes[i].first_child;
        u32 insert_pos = 0;
        while (child_idx != ~0u) {
            YGNodeInsertChild(yg_nodes[i], yg_nodes[child_idx], insert_pos);
            child_idx = nodes[child_idx].next_sibling;
            ++insert_pos;
        }
    }

    // Compute layout from root
    YGNodeCalculateLayout(yg_nodes[0], viewport.width, viewport.height, YGDirectionLTR);

    // Read back results into LayoutNodes (converting parent-relative -> absolute)
    readback_results(yg_nodes[0], nodes, 0, 0.0f, 0.0f);

    // Cleanup
    YGNodeFreeRecursive(yg_nodes[0]);
    YGConfigFree(config);
}

} // namespace ugui
