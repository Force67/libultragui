#pragma once

#include <ultragui/idl/parser.h>
#include <ultragui/style/style.h>
#include <ultragui/widgets/widget.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ugui {

class TextEngine;

/// Builds a widget tree from a parsed .ugui document.
/// Maps element types to widget constructors, resolves properties to styles.
class UguiBuilder {
public:
    using WidgetFactory = std::function<Widget*(const std::string& name)>;

    void register_type(const std::string& type_name, WidgetFactory factory);
    void set_text_engine(TextEngine* engine) { text_engine_ = engine; }

    /// Build a widget tree from a document. Caller owns the returned widgets.
    Widget* build(const UguiDocument& doc);

    /// Rebuild: diff against previous tree and patch in-place.
    /// Returns the root widget (may be the same or new).
    Widget* rebuild(const UguiDocument& doc, Widget* existing_root);

private:
    Widget* build_node(const UguiNode& node, u32& id_counter);
    void apply_properties(Widget* widget, const UguiNode& node);
    Style parse_style(const std::unordered_map<std::string, std::string>& props);

    std::unordered_map<std::string, WidgetFactory> factories_;
    TextEngine* text_engine_ = nullptr;
};

} // namespace ugui
