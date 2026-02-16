#include <ultragui/widgets/widget_tree.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

Widget* FindWidgetById(Widget* root, u32 id) {
    if (!root)
        return nullptr;
    if (root->id() == id)
        return root;
    for (auto* child : root->children()) {
        if (auto* found = FindWidgetById(child, id))
            return found;
    }
    return nullptr;
}

Widget* FindWidget(Widget* root, const char* name) {
    if (!root)
        return nullptr;
    if (root->name() == name)
        return root;
    for (auto* child : root->children()) {
        if (auto* found = FindWidget(child, name))
            return found;
    }
    return nullptr;
}

void UpdateWidgetTree(Widget* root, f64 dt) {
    if (!root)
        return;
    root->OnUpdate(dt);
    for (auto* child : root->children())
        UpdateWidgetTree(child, dt);
}

void MeasureWidgetTree(Widget* root) {
    if (!root)
        return;
    for (auto* child : root->children())
        MeasureWidgetTree(child);
    f32 w = 0, h = 0;
    root->Measure(w, h);
    root->set_intrinsic_size(w, h);
}

} // namespace ugui
