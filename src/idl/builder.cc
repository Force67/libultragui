#include <ultragui/animation/animator.h>
#include <ultragui/idl/builder.h>
#include <ultragui/widgets/button.h>
#include <ultragui/widgets/image.h>
#include <ultragui/widgets/panel.h>
#include <ultragui/widgets/scroll_view.h>
#include <ultragui/widgets/text.h>

#include <algorithm>
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
        return Length::Auto();
    f32 v = 0;
    auto end = std::from_chars(s.data(), s.data() + s.size(), v).ptr;
    std::string unit(end, s.data() + s.size());
    if (unit == "%")
        return Length::Percent(v);
    if (unit == "vw")
        return Length::Vw(v);
    if (unit == "vh")
        return Length::Vh(v);
    if (unit == "fr" || unit == "frac")
        return Length::Frac(v);
    // Bare decimal 0.0-1.0 without unit -> treat as fractional
    if (unit.empty() && v > 0.0f && v <= 1.0f && s.find('.') != std::string::npos)
        return Length::Frac(v);
    return Length::Px(v);
}

static Color parse_color(const std::string& s) {
    if (s.size() > 1 && s[0] == '#') {
        u32 hex = 0;
        std::from_chars(s.data() + 1, s.data() + s.size(), hex, 16);
        if (s.size() == 7)
            return Color::FromHex(hex);
        if (s.size() == 9) {
            u32 rgb = hex >> 8;
            f32 alpha = static_cast<f32>(hex & 0xFF) / 255.0f;
            return Color::FromHex(rgb, alpha);
        }
    }
    if (s == "white")
        return Color::White();
    if (s == "black")
        return Color::Black();
    if (s == "red")
        return Color::Red();
    if (s == "green")
        return Color::Green();
    if (s == "blue")
        return Color::Blue();
    if (s == "transparent")
        return Color::Transparent();
    return Color::White();
}

// ---------------------------------------------------------------------------
// Style parsing
// ---------------------------------------------------------------------------

Style UguiBuilder::ParseStyle(const std::unordered_map<std::string, std::string>& props) {
    Style s;

    for (auto& [key, val] : props) {
        if (key == "layout" || key == "flex-direction") {
            if (val == "row")
                s.flex_direction = FlexDirection::kRow;
            else if (val == "column")
                s.flex_direction = FlexDirection::kColumn;
            else if (val == "row-reverse")
                s.flex_direction = FlexDirection::kRowReverse;
            else if (val == "column-reverse")
                s.flex_direction = FlexDirection::kColumnReverse;
        } else if (key == "justify" || key == "justify-content") {
            if (val == "start")
                s.justify_content = JustifyContent::kStart;
            else if (val == "end")
                s.justify_content = JustifyContent::kEnd;
            else if (val == "center")
                s.justify_content = JustifyContent::kCenter;
            else if (val == "space-between")
                s.justify_content = JustifyContent::kSpaceBetween;
            else if (val == "space-around")
                s.justify_content = JustifyContent::kSpaceAround;
            else if (val == "space-evenly")
                s.justify_content = JustifyContent::kSpaceEvenly;
        } else if (key == "align" || key == "align-items") {
            if (val == "start")
                s.align_items = AlignItems::kStart;
            else if (val == "end")
                s.align_items = AlignItems::kEnd;
            else if (val == "center")
                s.align_items = AlignItems::kCenter;
            else if (val == "stretch")
                s.align_items = AlignItems::kStretch;
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
            f32 r = parse_float(val);
            s.corner_radius = r;
            s.corner_radius_tl = r;
            s.corner_radius_tr = r;
            s.corner_radius_br = r;
            s.corner_radius_bl = r;
        } else if (key == "corner-radius-tl") {
            s.corner_radius_tl = parse_float(val);
        } else if (key == "corner-radius-tr") {
            s.corner_radius_tr = parse_float(val);
        } else if (key == "corner-radius-br") {
            s.corner_radius_br = parse_float(val);
        } else if (key == "corner-radius-bl") {
            s.corner_radius_bl = parse_float(val);
        } else if (key == "opacity") {
            s.opacity = parse_float(val);
        } else if (key == "font-size") {
            s.font_size = parse_float(val);
        } else if (key == "text-align") {
            if (val == "left")
                s.text_align = TextAlign::kLeft;
            else if (val == "center")
                s.text_align = TextAlign::kCenter;
            else if (val == "right")
                s.text_align = TextAlign::kRight;
        } else if (key == "overflow") {
            if (val == "visible")
                s.overflow = Overflow::kVisible;
            else if (val == "hidden")
                s.overflow = Overflow::kHidden;
            else if (val == "scroll")
                s.overflow = Overflow::kScroll;
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
        } else if (key == "letter-spacing") {
            s.letter_spacing = parse_float(val);
        } else if (key == "line-height") {
            s.line_height_multiplier = parse_float(val);
        } else if (key == "text-transform") {
            if (val == "uppercase")
                s.text_transform = TextTransform::kUppercase;
            else if (val == "lowercase")
                s.text_transform = TextTransform::kLowercase;
            else if (val == "capitalize")
                s.text_transform = TextTransform::kCapitalize;
            else
                s.text_transform = TextTransform::kNone;
        } else if (key == "text-shadow-color") {
            s.text_shadow_color = parse_color(val);
        } else if (key == "text-shadow-blur") {
            s.text_shadow_blur = parse_float(val);
        } else if (key == "text-shadow-x") {
            s.text_shadow_offset.x = parse_float(val);
        } else if (key == "text-shadow-y") {
            s.text_shadow_offset.y = parse_float(val);
        } else if (key == "cursor") {
            if (val == "default")
                s.cursor = Cursor::kDefault;
            else if (val == "pointer")
                s.cursor = Cursor::kPointer;
            else if (val == "text")
                s.cursor = Cursor::kText;
            else if (val == "move")
                s.cursor = Cursor::kMove;
            else if (val == "not-allowed")
                s.cursor = Cursor::kNotAllowed;
            else
                s.cursor = Cursor::kAuto;
        } else if (key == "aspect-ratio") {
            s.aspect_ratio = parse_float(val);
        }
    }

    return s;
}

// ---------------------------------------------------------------------------
// Widget factory registration
// ---------------------------------------------------------------------------

void UguiBuilder::RegisterType(const std::string& type_name, WidgetFactory factory) {
    factories_[type_name] = std::move(factory);
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

Widget* UguiBuilder::Build(const UguiDocument& doc) {
    if (doc.roots.empty())
        return nullptr;

    u32 id_counter = 1;
    if (doc.roots.size() == 1) {
        return BuildNode(doc.roots[0], id_counter);
    }

    // Multiple roots: wrap in a panel
    auto* root = new Panel(0);
    root->set_name("_root");
    for (auto& node : doc.roots) {
        auto* child = BuildNode(node, id_counter);
        if (child)
            root->AddChild(child);
    }
    return root;
}

Widget* UguiBuilder::BuildNode(const UguiNode& node, u32& id_counter) {
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
        auto text_it = node.properties.find("content");
        if (text_it == node.properties.end())
            text_it = node.properties.find("text");
        if (text_it != node.properties.end())
            text->set_text(text_it->second);
        widget = text;
    } else if (node.type == "button") {
        auto* btn = new Button(id);
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

    ApplyProperties(widget, node);

    for (auto& child_node : node.children) {
        auto* child = BuildNode(child_node, id_counter);
        if (child)
            widget->AddChild(child);
    }

    return widget;
}

void UguiBuilder::ApplyProperties(Widget* widget, const UguiNode& node) {
    Style s = ParseStyle(node.properties);
    widget->set_style(s);

    // State overrides
    for (auto& sb : node.state_blocks) {
        WidgetState state = WidgetState::kNone;
        if (sb.state == "hover" || sb.state == "hovered")
            state = WidgetState::kHovered;
        else if (sb.state == "pressed" || sb.state == "active")
            state = WidgetState::kPressed;
        else if (sb.state == "focused" || sb.state == "focus")
            state = WidgetState::kFocused;
        else if (sb.state == "disabled")
            state = WidgetState::kDisabled;
        else if (sb.state == "checked")
            state = WidgetState::kChecked;

        Style override_style = ParseStyle(sb.properties);

        // Build mask from which properties are set
        u64 mask = 0;
        for (auto& [key, _] : sb.properties) {
            if (key == "background")
                mask |= StyleMask::kBackground;
            else if (key == "border-color")
                mask |= StyleMask::kBorderColor;
            else if (key == "border-width")
                mask |= StyleMask::kBorderWidth;
            else if (key == "border-radius" || key == "corner-radius" ||
                     key == "corner-radius-tl" || key == "corner-radius-tr" ||
                     key == "corner-radius-br" || key == "corner-radius-bl")
                mask |= StyleMask::kCornerRadius;
            else if (key == "opacity")
                mask |= StyleMask::kOpacity;
            else if (key == "color" || key == "text-color")
                mask |= StyleMask::kTextColor;
            else if (key == "font-size")
                mask |= StyleMask::kFontSize;
            else if (key == "width")
                mask |= StyleMask::kWidth;
            else if (key == "height")
                mask |= StyleMask::kHeight;
            else if (key == "background-end" || key == "gradient-end")
                mask |= StyleMask::kBackgroundEnd;
            else if (key == "shadow-color" || key == "shadow-blur" || key == "shadow-spread" ||
                     key == "shadow-x" || key == "shadow-y")
                mask |= StyleMask::kShadow;
        }

        widget->AddStateOverride(state, override_style, mask);

        // Parse transition properties
        Transition trans;
        bool has_transition = false;
        auto t_it = sb.properties.find("transition");
        auto td_it = sb.properties.find("transition-duration");
        auto te_it = sb.properties.find("transition-easing");
        auto tl_it = sb.properties.find("transition-delay");

        if (t_it != sb.properties.end()) {
            // Shorthand: "0.3s ease-in-out" or "0.3s ease-in-out 0.1s"
            has_transition = true;
            const auto& val = t_it->second;
            // Parse duration (first token)
            f32 dur = 0;
            const char* p = val.c_str();
            auto r = std::from_chars(p, p + val.size(), dur);
            if (r.ptr != p) {
                if (*r.ptr == 'm' && *(r.ptr + 1) == 's')
                    trans.duration = dur / 1000.0f;
                else
                    trans.duration = dur; // assume seconds
                p = r.ptr;
                while (*p && (*p == 's' || *p == 'm' || *p == ' '))
                    ++p;
            }
            // Parse optional easing
            std::string rest(p);
            if (rest.find("ease-in-out") != std::string::npos)
                trans.easing = EasingType::kEaseInOut;
            else if (rest.find("ease-in") != std::string::npos)
                trans.easing = EasingType::kEaseIn;
            else if (rest.find("ease-out") != std::string::npos)
                trans.easing = EasingType::kEaseOut;
            else if (rest.find("linear") != std::string::npos)
                trans.easing = EasingType::kLinear;
        }
        if (td_it != sb.properties.end()) {
            has_transition = true;
            f32 dur = 0;
            const auto& val = td_it->second;
            std::from_chars(val.data(), val.data() + val.size(), dur);
            trans.duration = val.find("ms") != std::string::npos ? dur / 1000.0f : dur;
        }
        if (te_it != sb.properties.end()) {
            const auto& val = te_it->second;
            if (val == "ease-in-out") trans.easing = EasingType::kEaseInOut;
            else if (val == "ease-in") trans.easing = EasingType::kEaseIn;
            else if (val == "ease-out") trans.easing = EasingType::kEaseOut;
            else if (val == "linear") trans.easing = EasingType::kLinear;
        }
        if (tl_it != sb.properties.end()) {
            f32 del = 0;
            const auto& val = tl_it->second;
            std::from_chars(val.data(), val.data() + val.size(), del);
            trans.delay = val.find("ms") != std::string::npos ? del / 1000.0f : del;
        }
        if (has_transition && state != WidgetState::kNone)
            widget->AddStateTransition(state, trans);
    }

    // @keyframes blocks
    for (auto& kb : node.keyframe_blocks) {
        if (!animator_ || kb.stops.empty())
            continue;

        KeyframeAnimation anim;
        anim.widget_id = widget->id();

        // Parse top-level properties
        auto d_it = kb.properties.find("duration");
        if (d_it != kb.properties.end()) {
            f32 dur = 0;
            std::from_chars(d_it->second.data(), d_it->second.data() + d_it->second.size(), dur);
            anim.duration = d_it->second.find("ms") != std::string::npos ? dur / 1000.0f : dur;
        }
        auto l_it = kb.properties.find("loop");
        if (l_it != kb.properties.end())
            anim.repeat_count = (l_it->second == "true") ? -1 : 1;
        auto a_it = kb.properties.find("alternate");
        if (a_it != kb.properties.end())
            anim.alternate = (a_it->second == "true");

        // Parse stops into keyframes
        for (auto& stop : kb.stops) {
            Keyframe kf;
            kf.time = stop.percent;
            kf.style = ParseStyle(stop.properties);
            anim.keyframes.push_back(kf);
        }

        // Sort keyframes by time
        std::sort(anim.keyframes.begin(), anim.keyframes.end(),
                  [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });

        anim.active = true;
        anim.start_time = 0; // Will be set when UIContext starts the animator
        animator_->StartAnimation(anim, 0);
    }
}

Widget* UguiBuilder::Rebuild(const UguiDocument& doc, Widget* existing_root) {
    // Simple rebuild: delete old tree and build new one
    // TODO: diff-based patching for hot reload
    delete existing_root;
    return Build(doc);
}

} // namespace ugui
