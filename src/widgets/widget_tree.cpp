#include <ultragui/widgets/widget_tree.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

Widget* find_widget(Widget* root, const char* name) {
    if (!root)
        return nullptr;
    if (root->name() == name)
        return root;
    for (u32 i = 0; i < root->child_count(); ++i) {
        Widget* found = find_widget(root->child_at(i), name);
        if (found)
            return found;
    }
    return nullptr;
}

void update_widget_tree(Widget* root, f64 dt) {
    if (!root)
        return;
    root->on_update(dt);
    for (u32 i = 0; i < root->child_count(); ++i) {
        update_widget_tree(root->child_at(i), dt);
    }
}

void measure_widget_tree(Widget* root) {
    if (!root)
        return;
    for (u32 i = 0; i < root->child_count(); ++i) {
        measure_widget_tree(root->child_at(i));
    }
    f32 w = 0, h = 0;
    root->measure(w, h);
    root->set_intrinsic_size(w, h);
}

} // namespace ugui
