#ifndef ULTRAGUI_IDL_BUILDER_H_
#define ULTRAGUI_IDL_BUILDER_H_

#include <ultragui/idl/parser.h>
#include <ultragui/style/style.h>
#include <ultragui/widgets/widget.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ugui {

class Animator;

/// Builds a widget tree from a parsed .ugui document.
/// Maps element types to widget constructors, resolves properties to styles.
class UguiBuilder {
public:
    using WidgetFactory = std::function<Widget*(const std::string& name)>;

    void RegisterType(const std::string& type_name, WidgetFactory factory);
    void set_animator(Animator* a) { animator_ = a; }

    /// Build a widget tree from a document. Caller owns the returned widgets.
    Widget* Build(const UguiDocument& doc);

    /// Rebuild: diff against previous tree and patch in-place.
    /// Returns the root widget (may be the same or new).
    Widget* Rebuild(const UguiDocument& doc, Widget* existing_root);

private:
    Widget* BuildNode(const UguiNode& node, u32& id_counter);
    void ApplyProperties(Widget* widget, const UguiNode& node);
    Style ParseStyle(const std::unordered_map<std::string, std::string>& props);

    std::unordered_map<std::string, WidgetFactory> factories_;
    Animator* animator_ = nullptr;
};

} // namespace ugui

#endif  // ULTRAGUI_IDL_BUILDER_H_
