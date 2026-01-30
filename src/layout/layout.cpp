#include <ultragui/layout/layout.h>

#include <algorithm>
#include <cmath>

namespace ugui {

static f32 resolve_length(const Length& len, f32 parent_size, const LayoutViewport& vp) {
    return len.resolve(parent_size, vp.width, vp.height);
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

void LayoutEngine::compute(LayoutNode* nodes, u32 node_count, const LayoutViewport& viewport) {
    if (node_count == 0)
        return;

    // Root node fills the viewport
    auto& root = nodes[0];
    root.computed_rect = {0, 0, viewport.width, viewport.height};
    root.computed_padding = {
        resolve_length(root.style.padding.top != 0 ? Length::px(root.style.padding.top)
                                                   : Length::px(0),
                       viewport.height, viewport),
        resolve_length(Length::px(root.style.padding.right), viewport.width, viewport),
        resolve_length(Length::px(root.style.padding.bottom), viewport.height, viewport),
        resolve_length(Length::px(root.style.padding.left), viewport.width, viewport),
    };
    root.computed_padding = root.style.padding;
    root.content_rect = {
        root.computed_rect.x + root.computed_padding.left,
        root.computed_rect.y + root.computed_padding.top,
        root.computed_rect.w - root.computed_padding.horizontal(),
        root.computed_rect.h - root.computed_padding.vertical(),
    };

    compute_node(nodes, 0, viewport.width, viewport.height, viewport);
}

// ---------------------------------------------------------------------------
// Per-node layout
// ---------------------------------------------------------------------------

void LayoutEngine::compute_node(LayoutNode* nodes, u32 node_index, f32 available_width,
                                f32 available_height, const LayoutViewport& viewport) {
    auto& node = nodes[node_index];

    // Resolve padding
    node.computed_padding = node.style.padding;

    // Resolve own width/height
    f32 width = available_width;
    f32 height = available_height;

    if (!node.style.width.is_auto()) {
        width = resolve_length(node.style.width, available_width, viewport);
    }
    if (!node.style.height.is_auto()) {
        height = resolve_length(node.style.height, available_height, viewport);
    }

    // Apply min/max constraints
    width = std::clamp(width, resolve_length(node.style.min_width, available_width, viewport),
                       resolve_length(node.style.max_width, available_width, viewport));
    height = std::clamp(height, resolve_length(node.style.min_height, available_height, viewport),
                        resolve_length(node.style.max_height, available_height, viewport));

    node.computed_rect.w = width;
    node.computed_rect.h = height;

    // Content rect (inside padding)
    f32 cw = width - node.computed_padding.horizontal();
    f32 ch = height - node.computed_padding.vertical();
    node.content_rect = {
        node.computed_rect.x + node.computed_padding.left,
        node.computed_rect.y + node.computed_padding.top,
        std::max(cw, 0.0f),
        std::max(ch, 0.0f),
    };

    // Layout children using flexbox
    if (node.first_child != ~0u) {
        layout_flex(nodes, node_index, node.content_rect.w, node.content_rect.h, viewport);
    } else if (node.style.width.is_auto()) {
        // Leaf node with auto width - use intrinsic size
        node.computed_rect.w = node.intrinsic_width + node.computed_padding.horizontal();
        node.content_rect.w = node.intrinsic_width;
    }
    if (node.style.height.is_auto() && node.first_child == ~0u) {
        node.computed_rect.h = node.intrinsic_height + node.computed_padding.vertical();
        node.content_rect.h = node.intrinsic_height;
    }

    node.layout_dirty = false;
}

// ---------------------------------------------------------------------------
// Flexbox layout
// ---------------------------------------------------------------------------

void LayoutEngine::layout_flex(LayoutNode* nodes, u32 parent_index, f32 container_width,
                               f32 container_height, const LayoutViewport& viewport) {
    auto& parent = nodes[parent_index];
    bool is_row = parent.style.flex_direction == FlexDirection::Row ||
                  parent.style.flex_direction == FlexDirection::RowReverse;
    bool is_reverse = parent.style.flex_direction == FlexDirection::RowReverse ||
                      parent.style.flex_direction == FlexDirection::ColumnReverse;

    f32 main_size = is_row ? container_width : container_height;
    f32 cross_size = is_row ? container_height : container_width;
    f32 gap = parent.style.gap;

    // First pass: measure children and resolve sizes
    struct ChildInfo {
        u32 index;
        f32 main_basis; // size along main axis
        f32 cross_basis;
        f32 flex_grow;
        f32 flex_shrink;
        EdgeInsets margin;
    };

    std::vector<ChildInfo> children;
    u32 child = parent.first_child;
    while (child != ~0u) {
        auto& c = nodes[child];
        c.computed_margin = c.style.margin;

        f32 child_main = 0, child_cross = 0;

        if (is_row) {
            if (!c.style.width.is_auto())
                child_main = resolve_length(c.style.width, container_width, viewport);
            else
                child_main = c.intrinsic_width + c.style.padding.horizontal();

            if (!c.style.height.is_auto())
                child_cross = resolve_length(c.style.height, container_height, viewport);
            else
                child_cross = 0; // will stretch
        } else {
            if (!c.style.height.is_auto())
                child_main = resolve_length(c.style.height, container_height, viewport);
            else
                child_main = c.intrinsic_height + c.style.padding.vertical();

            if (!c.style.width.is_auto())
                child_cross = resolve_length(c.style.width, container_width, viewport);
            else
                child_cross = 0;
        }

        children.push_back({child, child_main, child_cross, c.style.flex_grow, c.style.flex_shrink,
                            c.computed_margin});
        child = c.next_sibling;
    }

    if (children.empty())
        return;

    // Compute total main-axis usage
    f32 total_main = 0;
    f32 total_grow = 0;
    f32 total_shrink = 0;
    for (auto& ci : children) {
        f32 margin_main = is_row ? ci.margin.horizontal() : ci.margin.vertical();
        total_main += ci.main_basis + margin_main;
        total_grow += ci.flex_grow;
        total_shrink += ci.flex_shrink;
    }
    total_main += gap * static_cast<f32>(children.size() - 1);

    f32 free_space = main_size - total_main;

    // Distribute flex grow/shrink
    if (free_space > 0 && total_grow > 0) {
        for (auto& ci : children) {
            ci.main_basis += (ci.flex_grow / total_grow) * free_space;
        }
    } else if (free_space < 0 && total_shrink > 0) {
        for (auto& ci : children) {
            ci.main_basis += (ci.flex_shrink / total_shrink) * free_space;
        }
    }

    // Compute justify-content spacing
    f32 justify_offset = 0;
    f32 justify_gap = gap;
    f32 remaining = main_size - total_main + free_space; // after flex adjustment
    // Recompute remaining after flex
    remaining = main_size;
    for (auto& ci : children) {
        f32 margin_main = is_row ? ci.margin.horizontal() : ci.margin.vertical();
        remaining -= ci.main_basis + margin_main;
    }
    remaining -= gap * static_cast<f32>(children.size() - 1);

    switch (parent.style.justify_content) {
    case JustifyContent::Start:
        break;
    case JustifyContent::End:
        justify_offset = remaining;
        break;
    case JustifyContent::Center:
        justify_offset = remaining * 0.5f;
        break;
    case JustifyContent::SpaceBetween:
        if (children.size() > 1)
            justify_gap = gap + remaining / static_cast<f32>(children.size() - 1);
        break;
    case JustifyContent::SpaceAround:
        if (!children.empty()) {
            f32 sp = remaining / static_cast<f32>(children.size());
            justify_offset = sp * 0.5f;
            justify_gap = gap + sp;
        }
        break;
    case JustifyContent::SpaceEvenly:
        if (!children.empty()) {
            f32 sp = remaining / static_cast<f32>(children.size() + 1);
            justify_offset = sp;
            justify_gap = gap + sp;
        }
        break;
    }

    // Determine if the cross axis is auto-sized.
    // Per flexbox spec, stretch has no effect when the container's cross size is auto.
    bool cross_auto = is_row ? parent.style.height.is_auto() : parent.style.width.is_auto();

    // Position children
    f32 main_cursor = justify_offset;
    f32 content_x = parent.content_rect.x;
    f32 content_y = parent.content_rect.y;
    f32 auto_cross = 0.0f;

    for (usize i = 0; i < children.size(); ++i) {
        auto& ci = children[is_reverse ? children.size() - 1 - i : i];
        auto& c = nodes[ci.index];

        f32 margin_before = is_row ? ci.margin.left : ci.margin.top;
        f32 margin_after = is_row ? ci.margin.right : ci.margin.bottom;
        f32 margin_cross_before = is_row ? ci.margin.top : ci.margin.left;

        main_cursor += margin_before;

        // Resolve cross-axis size
        f32 child_cross = ci.cross_basis;
        AlignItems align = parent.style.align_items;
        AlignSelf self_align = c.style.align_self;
        if (self_align != AlignSelf::Auto) {
            align = static_cast<AlignItems>(static_cast<u8>(self_align) - 1);
        }

        // Don't stretch when container cross size is auto - children use natural size first
        if (child_cross <= 0 && align == AlignItems::Stretch && !cross_auto) {
            f32 cross_margin = is_row ? ci.margin.vertical() : ci.margin.horizontal();
            child_cross = cross_size - cross_margin;
        }

        if (is_row) {
            c.computed_rect = {content_x + main_cursor, content_y + margin_cross_before,
                               ci.main_basis, child_cross};
        } else {
            c.computed_rect = {content_x + margin_cross_before, content_y + main_cursor,
                               child_cross, ci.main_basis};
        }

        // Recurse into children - this may shrink-wrap auto-sized dimensions
        compute_node(nodes, ci.index, c.computed_rect.w, c.computed_rect.h, viewport);

        // Track actual cross size after compute_node (may have shrink-wrapped)
        f32 actual_cross = is_row ? c.computed_rect.h : c.computed_rect.w;
        f32 cross_margin = is_row ? ci.margin.vertical() : ci.margin.horizontal();
        if (actual_cross + cross_margin > auto_cross)
            auto_cross = actual_cross + cross_margin;

        main_cursor += ci.main_basis + margin_after + justify_gap;
    }

    // If parent has auto height, shrink-wrap to content
    if (parent.style.height.is_auto()) {
        if (is_row) {
            parent.computed_rect.h = auto_cross + parent.computed_padding.vertical();
        } else {
            parent.computed_rect.h = main_cursor - justify_gap + parent.computed_padding.vertical();
        }
        parent.content_rect.h = parent.computed_rect.h - parent.computed_padding.vertical();
    }
    if (parent.style.width.is_auto() && !is_row) {
        parent.computed_rect.w = auto_cross + parent.computed_padding.horizontal();
        parent.content_rect.w = auto_cross;
    }

    // Second pass: apply stretch and cross-axis alignment now that we know the final cross size
    f32 final_cross = is_row ? parent.content_rect.h : parent.content_rect.w;
    for (auto& ci : children) {
        auto& c = nodes[ci.index];
        AlignItems align = parent.style.align_items;
        AlignSelf self_align = c.style.align_self;
        if (self_align != AlignSelf::Auto)
            align = static_cast<AlignItems>(static_cast<u8>(self_align) - 1);

        f32 child_cross_size = is_row ? c.computed_rect.h : c.computed_rect.w;
        f32 margin_cross_before = is_row ? ci.margin.top : ci.margin.left;
        f32 cross_margin = is_row ? ci.margin.vertical() : ci.margin.horizontal();

        f32 cross_pos = margin_cross_before;
        bool needs_stretch = false;

        switch (align) {
        case AlignItems::Start:
            break;
        case AlignItems::End:
            cross_pos = final_cross - child_cross_size -
                        (is_row ? ci.margin.bottom : ci.margin.right);
            break;
        case AlignItems::Center:
            cross_pos = (final_cross - child_cross_size - cross_margin) * 0.5f + margin_cross_before;
            break;
        case AlignItems::Stretch:
            if (ci.cross_basis <= 0) {
                child_cross_size = final_cross - cross_margin;
                needs_stretch = true;
            }
            break;
        case AlignItems::Baseline:
            break;
        }

        if (is_row) {
            c.computed_rect.y = parent.content_rect.y + cross_pos;
            if (needs_stretch)
                c.computed_rect.h = child_cross_size;
        } else {
            c.computed_rect.x = parent.content_rect.x + cross_pos;
            if (needs_stretch)
                c.computed_rect.w = child_cross_size;
        }

        // Re-compute content rect if stretched
        if (needs_stretch) {
            c.computed_padding = c.style.padding;
            c.content_rect = {
                c.computed_rect.x + c.computed_padding.left,
                c.computed_rect.y + c.computed_padding.top,
                std::max(c.computed_rect.w - c.computed_padding.horizontal(), 0.0f),
                std::max(c.computed_rect.h - c.computed_padding.vertical(), 0.0f),
            };
        }
    }
}

} // namespace ugui
