#include <ultragui/idl/builder.h>
#include <ultragui/text/text_engine.h>
#include <ultragui/widgets/button.h>
#include <ultragui/widgets/image.h>
#include <ultragui/widgets/panel.h>
#include <ultragui/widgets/scroll_view.h>
#include <ultragui/widgets/text.h>

#include <charconv>
#include <cstdio>

namespace ugui {

static f32 parse_float(const std::string& s) {
    f32 v = 0;
    std::from_chars(s.data(), s.data() + s.size(), v);
    return v;
}

static EdgeInsets parse_edge_insets(const std::string& s) {
    // Parse 1, 2, or 4 values like CSS shorthand:
    // "10" -> all sides = 10
    // "10 20" -> vertical=10, horizontal=20
    // "10 20 30 40" -> top=10, right=20, bottom=30, left=40
    f32 vals[4] = {};
    int count = 0;
    const char* ptr = s.data();
    const char* end = s.data() + s.size();
    while (ptr < end && count < 4) {
        while (ptr < end && (*ptr == ' ' || *ptr == '\t'))
            ++ptr;
        if (ptr >= end)
            break;
        f32 v = 0;
        auto result = std::from_chars(ptr, end, v);
        if (result.ptr == ptr)
            break;
        vals[count++] = v;
        ptr = result.ptr;
    }

    if (count == 1)
        return EdgeInsets(vals[0]);
    if (count == 2)
        return EdgeInsets(vals[0], vals[1]);
    if (count >= 4)
        return EdgeInsets(vals[0], vals[1], vals[2], vals[3]);
    return EdgeInsets(vals[0]);
}

static Length parse_length(const std::string& s) {
    if (s == "auto")
        return Length::auto_();
    f32 v = 0;
    auto end = std::from_chars(s.data(), s.data() + s.size(), v).ptr;
    std::string unit(end, s.data() + s.size());
    if (unit == "%")
        return Length::percent(v);
    if (unit == "vw")
        return Length::vw(v);
    if (unit == "vh")
        return Length::vh(v);
    if (unit == "fr" || unit == "frac")
        return Length::frac(v);
    // Bare decimal 0.0-1.0 without unit -> treat as fractional
    if (unit.empty() && v > 0.0f && v <= 1.0f && s.find('.') != std::string::npos)
        return Length::frac(v);
    return Length::px(v);
}

static Color parse_color(const std::string& s) {
    if (s.size() > 1 && s[0] == '#') {
        u32 hex = 0;
        std::from_chars(s.data() + 1, s.data() + s.size(), hex, 16);
        if (s.size() == 7)
            return Color::from_hex(hex);
        if (s.size() == 9) {
            u32 rgb = hex >> 8;
            f32 alpha = static_cast<f32>(hex & 0xFF) / 255.0f;
            return Color::from_hex(rgb, alpha);
        }
    }
    if (s == "white")
        return Color::white();
    if (s == "black")
        return Color::black();
    if (s == "red")
        return Color::red();
    if (s == "green")
        return Color::green();
    if (s == "blue")
        return Color::blue();
    if (s == "transparent")
        return Color::transparent();
    return Color::white();
}

// ---------------------------------------------------------------------------
// Style parsing
// ---------------------------------------------------------------------------

Style UguiBuilder::parse_style(const std::unordered_map<std::string, std::string>& props) {
    Style s;

    for (auto& [key, val] : props) {
        if (key == "layout" || key == "flex-direction") {
            if (val == "row")
                s.flex_direction = FlexDirection::Row;
            else if (val == "column")
                s.flex_direction = FlexDirection::Column;
            else if (val == "row-reverse")
                s.flex_direction = FlexDirection::RowReverse;
            else if (val == "column-reverse")
                s.flex_direction = FlexDirection::ColumnReverse;
        } else if (key == "justify" || key == "justify-content") {
            if (val == "start")
                s.justify_content = JustifyContent::Start;
            else if (val == "end")
                s.justify_content = JustifyContent::End;
            else if (val == "center")
                s.justify_content = JustifyContent::Center;
            else if (val == "space-between")
                s.justify_content = JustifyContent::SpaceBetween;
            else if (val == "space-around")
                s.justify_content = JustifyContent::SpaceAround;
            else if (val == "space-evenly")
                s.justify_content = JustifyContent::SpaceEvenly;
        } else if (key == "align" || key == "align-items") {
            if (val == "start")
                s.align_items = AlignItems::Start;
            else if (val == "end")
                s.align_items = AlignItems::End;
            else if (val == "center")
                s.align_items = AlignItems::Center;
            else if (val == "stretch")
                s.align_items = AlignItems::Stretch;
        } else if (key == "width") {
            s.width = parse_length(val);
        } else if (key == "height") {
            s.height = parse_length(val);
        } else if (key == "min-width") {
            s.min_width = parse_length(val);
        } else if (key == "min-height") {
            s.min_height = parse_length(val);
        } else if (key == "max-width") {
            s.max_width = parse_length(val);
        } else if (key == "max-height") {
            s.max_height = parse_length(val);
        } else if (key == "flex-grow") {
            s.flex_grow = parse_float(val);
        } else if (key == "flex-shrink") {
            s.flex_shrink = parse_float(val);
        } else if (key == "padding") {
            s.padding = parse_edge_insets(val);
        } else if (key == "margin") {
            s.margin = parse_edge_insets(val);
        } else if (key == "gap") {
            s.gap = parse_float(val);
        } else if (key == "background") {
            s.background = parse_color(val);
        } else if (key == "color" || key == "text-color") {
            s.text_color = parse_color(val);
        } else if (key == "border-color") {
            s.border_color = parse_color(val);
        } else if (key == "border-width") {
            s.border_width = parse_float(val);
        } else if (key == "border-radius" || key == "corner-radius") {
            s.corner_radius = parse_float(val);
        } else if (key == "opacity") {
            s.opacity = parse_float(val);
        } else if (key == "font-size") {
            s.font_size = parse_float(val);
        } else if (key == "text-align") {
            if (val == "left")
                s.text_align = TextAlign::Left;
            else if (val == "center")
                s.text_align = TextAlign::Center;
            else if (val == "right")
                s.text_align = TextAlign::Right;
        } else if (key == "overflow") {
            if (val == "visible")
                s.overflow = Overflow::Visible;
            else if (val == "hidden")
                s.overflow = Overflow::Hidden;
            else if (val == "scroll")
                s.overflow = Overflow::Scroll;
        } else if (key == "background-end" || key == "gradient-end") {
            s.background_end = parse_color(val);
        } else if (key == "shadow-color") {
            s.shadow.color = parse_color(val);
        } else if (key == "shadow-blur") {
            s.shadow.blur = parse_float(val);
        } else if (key == "shadow-spread") {
            s.shadow.spread = parse_float(val);
        } else if (key == "shadow-x") {
            s.shadow.offset.x = parse_float(val);
        } else if (key == "shadow-y") {
            s.shadow.offset.y = parse_float(val);
        }
    }

    return s;
}

// ---------------------------------------------------------------------------
// Widget factory registration
// ---------------------------------------------------------------------------

void UguiBuilder::register_type(const std::string& type_name, WidgetFactory factory) {
    factories_[type_name] = std::move(factory);
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

Widget* UguiBuilder::build(const UguiDocument& doc) {
    if (doc.roots.empty())
        return nullptr;

    u32 id_counter = 1;
    if (doc.roots.size() == 1) {
        return build_node(doc.roots[0], id_counter);
    }

    // Multiple roots: wrap in a panel
    auto* root = new Panel(0);
    root->set_name("_root");
    for (auto& node : doc.roots) {
        auto* child = build_node(node, id_counter);
        if (child)
            root->add_child(child);
    }
    return root;
}

Widget* UguiBuilder::build_node(const UguiNode& node, u32& id_counter) {
    Widget* widget = nullptr;
    u32 id = id_counter++;

    // Try registered factory
    auto it = factories_.find(node.type);
    if (it != factories_.end()) {
        widget = it->second(node.name);
    } else if (node.type == "panel" || node.type == "div" || node.type == "container") {
        widget = new Panel(id);
    } else if (node.type == "text" || node.type == "label") {
        auto* text = new Text(id);
        text->set_text_engine(text_engine_);
        auto text_it = node.properties.find("content");
        if (text_it == node.properties.end())
            text_it = node.properties.find("text");
        if (text_it != node.properties.end())
            text->set_text(text_it->second);
        widget = text;
    } else if (node.type == "button") {
        auto* btn = new Button(id);
        btn->set_text_engine(text_engine_);
        auto text_it = node.properties.find("text");
        if (text_it == node.properties.end())
            text_it = node.properties.find("label");
        if (text_it != node.properties.end())
            btn->set_label(text_it->second);
        widget = btn;
    } else if (node.type == "image" || node.type == "img") {
        widget = new Image(id);
    } else if (node.type == "scroll" || node.type == "scroll-view") {
        widget = new ScrollView(id);
    } else {
        // Unknown type - treat as panel
        std::fprintf(stderr, "ultragui: unknown element type '%s' at line %u\n", node.type.c_str(),
                     node.source_line);
        widget = new Panel(id);
    }

    widget->set_id(id);
    widget->set_name(node.name);

    apply_properties(widget, node);

    for (auto& child_node : node.children) {
        auto* child = build_node(child_node, id_counter);
        if (child)
            widget->add_child(child);
    }

    return widget;
}

void UguiBuilder::apply_properties(Widget* widget, const UguiNode& node) {
    Style s = parse_style(node.properties);
    widget->set_style(s);

    // State overrides
    for (auto& sb : node.state_blocks) {
        WidgetState state = WidgetState::None;
        if (sb.state == "hover" || sb.state == "hovered")
            state = WidgetState::Hovered;
        else if (sb.state == "pressed" || sb.state == "active")
            state = WidgetState::Pressed;
        else if (sb.state == "focused" || sb.state == "focus")
            state = WidgetState::Focused;
        else if (sb.state == "disabled")
            state = WidgetState::Disabled;
        else if (sb.state == "checked")
            state = WidgetState::Checked;

        Style override_style = parse_style(sb.properties);

        // Build mask from which properties are set
        u64 mask = 0;
        for (auto& [key, _] : sb.properties) {
            if (key == "background")
                mask |= StyleMask::Background;
            else if (key == "border-color")
                mask |= StyleMask::BorderColor;
            else if (key == "border-width")
                mask |= StyleMask::BorderWidth;
            else if (key == "border-radius" || key == "corner-radius")
                mask |= StyleMask::CornerRadius;
            else if (key == "opacity")
                mask |= StyleMask::Opacity;
            else if (key == "color" || key == "text-color")
                mask |= StyleMask::TextColor;
            else if (key == "font-size")
                mask |= StyleMask::FontSize;
            else if (key == "width")
                mask |= StyleMask::Width;
            else if (key == "height")
                mask |= StyleMask::Height;
            else if (key == "background-end" || key == "gradient-end")
                mask |= StyleMask::BackgroundEnd;
            else if (key == "shadow-color" || key == "shadow-blur" || key == "shadow-spread" ||
                     key == "shadow-x" || key == "shadow-y")
                mask |= StyleMask::Shadow;
        }

        widget->add_state_override(state, override_style, mask);
    }
}

Widget* UguiBuilder::rebuild(const UguiDocument& doc, Widget* existing_root) {
    // Simple rebuild: delete old tree and build new one
    // TODO: diff-based patching for hot reload
    delete existing_root;
    return build(doc);
}

} // namespace ugui
