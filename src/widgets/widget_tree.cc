#include <ultragui/widgets/widget_tree.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

Widget* FindWidgetById(Widget* root, u32 id) {
    if (!root)
        return nullptr;
    if (root->id() == id)
        return root;
    for (u32 i = 0; i < root->child_count(); ++i) {
        Widget* found = FindWidgetById(root->ChildAt(i), id);
        if (found)
            return found;
    }
    return nullptr;
}

Widget* FindWidget(Widget* root, const char* name) {
    if (!root)
        return nullptr;
    if (root->name() == name)
        return root;
    for (u32 i = 0; i < root->child_count(); ++i) {
        Widget* found = FindWidget(root->ChildAt(i), name);
        if (found)
            return found;
    }
    return nullptr;
}

void UpdateWidgetTree(Widget* root, f64 dt) {
    if (!root)
        return;
    root->OnUpdate(dt);
    for (u32 i = 0; i < root->child_count(); ++i) {
        UpdateWidgetTree(root->ChildAt(i), dt);
    }
}

void MeasureWidgetTree(Widget* root) {
    if (!root)
        return;
    for (u32 i = 0; i < root->child_count(); ++i) {
        MeasureWidgetTree(root->ChildAt(i));
    }
    f32 w = 0, h = 0;
    root->Measure(w, h);
    root->set_intrinsic_size(w, h);
}

} // namespace ugui
