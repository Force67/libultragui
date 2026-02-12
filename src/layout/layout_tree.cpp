#include <ultragui/layout/layout_tree.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

static void build_layout_nodes(Widget* widget, u32 parent_index,
                               std::vector<LayoutNode>& nodes) {
    u32 my_index = static_cast<u32>(nodes.size());

    LayoutNode node;
    widget->populate_layout_node(node);
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
        build_layout_nodes(widget->child_at(i), my_index, nodes);
    }
}

static void apply_layout_results(Widget* widget, u32& node_index,
                                 const std::vector<LayoutNode>& nodes) {
    auto& node = nodes[node_index];
    widget->apply_layout_result(node);
    widget->on_layout(node.computed_rect, node.content_rect);

    ++node_index;
    for (u32 i = 0; i < widget->child_count(); ++i) {
        apply_layout_results(widget->child_at(i), node_index, nodes);
    }
}

void compute_widget_layout(Widget* root, const LayoutViewport& vp,
                           LayoutEngine& engine, std::vector<LayoutNode>& scratch) {
    if (!root)
        return;

    scratch.clear();
    build_layout_nodes(root, ~0u, scratch);

    engine.compute(scratch.data(), static_cast<u32>(scratch.size()), vp);

    u32 idx = 0;
    apply_layout_results(root, idx, scratch);
}

} // namespace ugui
