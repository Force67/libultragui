#include <ultragui/animation/animator.h>
#include <ultragui/idl/builder.h>
#include <ultragui/widgets/button.h>
#include <ultragui/widgets/checkbox.h>
#include <ultragui/widgets/context_menu.h>
#include <ultragui/widgets/dropdown.h>
#include <ultragui/widgets/image.h>
#include <ultragui/widgets/modal.h>
#include <ultragui/widgets/panel.h>
#include <ultragui/widgets/radio.h>
#include <ultragui/widgets/rich_text.h>
#include <ultragui/widgets/scroll_view.h>
#include <ultragui/widgets/slider.h>
#include <ultragui/widgets/text.h>
#include <ultragui/widgets/text_input.h>
#include <ultragui/widgets/toggle.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <iterator>
#include <numeric>
#include <optional>
#include <string_view>

namespace ugui {

// ---------------------------------------------------------------------------
// Pure parse helpers
// ---------------------------------------------------------------------------

static f32 parse_float(const String& s) {
    f32 v = 0;
    std::from_chars(s.data(), s.data() + s.size(), v);
    return v;
}

static EdgeInsets parse_edge_insets(const String& s) {
    // Parse 1, 2, or 4 values like CSS shorthand:
    //   "10"        -> all sides = 10
    //   "10 20"     -> vertical=10, horizontal=20
    //   "10 20 30 40" -> top=10, right=20, bottom=30, left=40
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

static Length parse_length(const String& s) {
    if (s == "auto")
        return Length::Auto();
    f32 v = 0;
    auto end = std::from_chars(s.data(), s.data() + s.size(), v).ptr;
    String unit(end, s.data() + s.size());
    if (unit == "%")
        return Length::Percent(v);
    if (unit == "vw")
        return Length::Vw(v);
    if (unit == "vh")
        return Length::Vh(v);
    if (unit == "fr" || unit == "frac")
        return Length::Frac(v);
    // Bare decimal 0.0-1.0 without unit -> treat as fractional
    if (unit.empty() && v > 0.0f && v <= 1.0f && s.find('.') != String::npos)
        return Length::Frac(v);
    return Length::Px(v);
}

static f32 parse_duration(const String& val) {
    f32 dur = 0;
    std::from_chars(val.data(), val.data() + val.size(), dur);
    return val.find("ms") != String::npos ? dur / 1000.0f : dur;
}

// ---------------------------------------------------------------------------
// Declarative enum/value lookup tables
// ---------------------------------------------------------------------------

template <typename E, std::size_t N>
static Optional<E> LookupEnum(const std::pair<std::string_view, E> (&table)[N],
                                   std::string_view key) {
    for (auto& [k, v] : table) {
        if (k == key)
            return v;
    }
    return std::nullopt;
}

static constexpr std::pair<std::string_view, FlexDirection> kFlexDirectionTable[] = {
    {"row", FlexDirection::kRow},
    {"column", FlexDirection::kColumn},
    {"row-reverse", FlexDirection::kRowReverse},
    {"column-reverse", FlexDirection::kColumnReverse},
};

static constexpr std::pair<std::string_view, JustifyContent> kJustifyContentTable[] = {
    {"start", JustifyContent::kStart},
    {"end", JustifyContent::kEnd},
    {"center", JustifyContent::kCenter},
    {"space-between", JustifyContent::kSpaceBetween},
    {"space-around", JustifyContent::kSpaceAround},
    {"space-evenly", JustifyContent::kSpaceEvenly},
};

static constexpr std::pair<std::string_view, AlignItems> kAlignItemsTable[] = {
    {"start", AlignItems::kStart},
    {"end", AlignItems::kEnd},
    {"center", AlignItems::kCenter},
    {"stretch", AlignItems::kStretch},
};

static constexpr std::pair<std::string_view, TextAlign> kTextAlignTable[] = {
    {"left", TextAlign::kLeft},
    {"center", TextAlign::kCenter},
    {"right", TextAlign::kRight},
};

static constexpr std::pair<std::string_view, Overflow> kOverflowTable[] = {
    {"visible", Overflow::kVisible},
    {"hidden", Overflow::kHidden},
    {"scroll", Overflow::kScroll},
};

static constexpr std::pair<std::string_view, TextTransform> kTextTransformTable[] = {
    {"uppercase", TextTransform::kUppercase},
    {"lowercase", TextTransform::kLowercase},
    {"capitalize", TextTransform::kCapitalize},
    {"none", TextTransform::kNone},
};

static constexpr std::pair<std::string_view, Cursor> kCursorTable[] = {
    {"default", Cursor::kDefault},
    {"pointer", Cursor::kPointer},
    {"text", Cursor::kText},
    {"move", Cursor::kMove},
    {"not-allowed", Cursor::kNotAllowed},
};

static constexpr std::pair<std::string_view, WidgetState> kWidgetStateTable[] = {
    {"hover", WidgetState::kHovered},
    {"hovered", WidgetState::kHovered},
    {"pressed", WidgetState::kPressed},
    {"active", WidgetState::kPressed},
    {"focused", WidgetState::kFocused},
    {"focus", WidgetState::kFocused},
    {"disabled", WidgetState::kDisabled},
    {"checked", WidgetState::kChecked},
};

static constexpr std::pair<std::string_view, EasingType> kEasingTable[] = {
    {"ease-in-out", EasingType::kEaseInOut},
    {"ease-in", EasingType::kEaseIn},
    {"ease-out", EasingType::kEaseOut},
    {"linear", EasingType::kLinear},
    {"spring", EasingType::kSpring},
};

// Easing substring match: order matters (ease-in-out before ease-in)
static Optional<EasingType> FindEasingSubstring(std::string_view text) {
    for (auto& [substr, easing] : kEasingTable) {
        if (text.find(substr) != std::string_view::npos)
            return easing;
    }
    return std::nullopt;
}

// Named color table
static constexpr std::pair<std::string_view, Color> kNamedColors[] = {
    {"white", Color::White()},
    {"black", Color::Black()},
    {"red", Color::Red()},
    {"green", Color::Green()},
    {"blue", Color::Blue()},
    {"transparent", Color::Transparent()},
};

static Color parse_color(const String& s) {
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
    return LookupEnum(kNamedColors, s).value_or(Color::White());
}

// ---------------------------------------------------------------------------
// Property dispatch table: maps CSS property names to Style mutators
// ---------------------------------------------------------------------------

using StyleSetter = void (*)(Style&, const String&);

static const std::pair<std::string_view, StyleSetter> kPropertyTable[] = {
    // Layout enums
    {"layout", [](Style& s, const String& v) {
        if (auto e = LookupEnum(kFlexDirectionTable, v)) s.flex_direction = *e;
    }},
    {"flex-direction", [](Style& s, const String& v) {
        if (auto e = LookupEnum(kFlexDirectionTable, v)) s.flex_direction = *e;
    }},
    {"justify", [](Style& s, const String& v) {
        if (auto e = LookupEnum(kJustifyContentTable, v)) s.justify_content = *e;
    }},
    {"justify-content", [](Style& s, const String& v) {
        if (auto e = LookupEnum(kJustifyContentTable, v)) s.justify_content = *e;
    }},
    {"align", [](Style& s, const String& v) {
        if (auto e = LookupEnum(kAlignItemsTable, v)) s.align_items = *e;
    }},
    {"align-items", [](Style& s, const String& v) {
        if (auto e = LookupEnum(kAlignItemsTable, v)) s.align_items = *e;
    }},
    {"text-align", [](Style& s, const String& v) {
        if (auto e = LookupEnum(kTextAlignTable, v)) s.text_align = *e;
    }},
    {"overflow", [](Style& s, const String& v) {
        if (auto e = LookupEnum(kOverflowTable, v)) s.overflow = *e;
    }},
    {"text-transform", [](Style& s, const String& v) {
        s.text_transform = LookupEnum(kTextTransformTable, v).value_or(TextTransform::kNone);
    }},
    {"font-weight", [](Style& s, const String& v) {
        static constexpr std::pair<std::string_view, FontWeight> kWeightTable[] = {
            {"thin", FontWeight::kThin},
            {"extra-light", FontWeight::kExtraLight},
            {"light", FontWeight::kLight},
            {"regular", FontWeight::kRegular},
            {"normal", FontWeight::kRegular},
            {"medium", FontWeight::kMedium},
            {"semi-bold", FontWeight::kSemiBold},
            {"semibold", FontWeight::kSemiBold},
            {"bold", FontWeight::kBold},
            {"extra-bold", FontWeight::kExtraBold},
            {"black", FontWeight::kBlack},
        };
        for (auto& [name, weight] : kWeightTable) {
            if (name == v) { s.font_weight = weight; return; }
        }
        // Try numeric
        f32 num = 0;
        std::from_chars(v.data(), v.data() + v.size(), num);
        if (num >= 100 && num <= 900)
            s.font_weight = static_cast<FontWeight>(static_cast<u16>(num));
    }},
    {"font-style", [](Style& s, const String& v) {
        if (v == "italic") s.font_style = FontStyle::kItalic;
        else s.font_style = FontStyle::kNormal;
    }},
    {"cursor", [](Style& s, const String& v) {
        s.cursor = LookupEnum(kCursorTable, v).value_or(Cursor::kAuto);
    }},
    {"position", [](Style& s, const String& v) {
        if (v == "relative") s.position = Position::kRelative;
        else if (v == "absolute") s.position = Position::kAbsolute;
        else if (v == "sticky") s.position = Position::kSticky;
    }},

    // Sizing
    {"width", [](Style& s, const String& v) { s.width = parse_length(v); }},
    {"height", [](Style& s, const String& v) { s.height = parse_length(v); }},
    {"min-width", [](Style& s, const String& v) { s.min_width = parse_length(v); }},
    {"min-height", [](Style& s, const String& v) { s.min_height = parse_length(v); }},
    {"max-width", [](Style& s, const String& v) { s.max_width = parse_length(v); }},
    {"max-height", [](Style& s, const String& v) { s.max_height = parse_length(v); }},

    // Flex
    {"flex-grow", [](Style& s, const String& v) { s.flex_grow = parse_float(v); }},
    {"flex-shrink", [](Style& s, const String& v) { s.flex_shrink = parse_float(v); }},

    // Spacing
    {"padding", [](Style& s, const String& v) { s.padding = parse_edge_insets(v); }},
    {"margin", [](Style& s, const String& v) { s.margin = parse_edge_insets(v); }},
    {"gap", [](Style& s, const String& v) { s.gap = parse_float(v); }},

    // Colors
    {"background", [](Style& s, const String& v) { s.background = parse_color(v); }},
    {"color", [](Style& s, const String& v) { s.text_color = parse_color(v); }},
    {"text-color", [](Style& s, const String& v) { s.text_color = parse_color(v); }},
    {"border-color", [](Style& s, const String& v) { s.border_color = parse_color(v); }},
    {"background-end", [](Style& s, const String& v) { s.background_end = parse_color(v); }},
    {"gradient-end", [](Style& s, const String& v) { s.background_end = parse_color(v); }},
    {"gradient-angle", [](Style& s, const String& v) { s.gradient_angle = parse_float(v); }},
    {"gradient-type", [](Style& s, const String& v) {
        if (v == "radial") s.gradient_type = GradientType::kRadial;
        else s.gradient_type = GradientType::kLinear;
    }},
    {"gradient-stops", [](Style& s, const String& v) {
        // Parse comma-separated "color position%" pairs
        // e.g. "#ff0000 0%, #00ff00 50%, #0000ff 100%"
        s.gradient_stop_count = 0;
        std::string_view sv(v);
        while (!sv.empty() && s.gradient_stop_count < Style::kMaxGradientStops) {
            // Skip whitespace and commas
            while (!sv.empty() && (sv[0] == ' ' || sv[0] == ','))
                sv.remove_prefix(1);
            if (sv.empty()) break;

            // Find the color token (starts with # or is a name)
            auto space = sv.find(' ');
            if (space == std::string_view::npos) break;
            String color_str(sv.substr(0, space));
            sv.remove_prefix(space + 1);

            // Skip whitespace
            while (!sv.empty() && sv[0] == ' ')
                sv.remove_prefix(1);

            // Parse position (number followed by optional %)
            f32 pos = 0;
            auto end = std::from_chars(sv.data(), sv.data() + sv.size(), pos);
            if (end.ptr == sv.data()) break;
            sv.remove_prefix(static_cast<size_t>(end.ptr - sv.data()));
            if (!sv.empty() && sv[0] == '%') {
                pos /= 100.0f;
                sv.remove_prefix(1);
            }

            auto& stop = s.gradient_stops[s.gradient_stop_count++];
            stop.position = pos;
            stop.color = parse_color(color_str);
        }
    }},

    // Border
    {"border-width", [](Style& s, const String& v) { s.border_width = parse_float(v); }},
    {"border-radius", [](Style& s, const String& v) {
        f32 r = parse_float(v);
        s.corner_radius = s.corner_radius_tl = s.corner_radius_tr =
            s.corner_radius_br = s.corner_radius_bl = r;
    }},
    {"corner-radius", [](Style& s, const String& v) {
        f32 r = parse_float(v);
        s.corner_radius = s.corner_radius_tl = s.corner_radius_tr =
            s.corner_radius_br = s.corner_radius_bl = r;
    }},
    {"corner-radius-tl", [](Style& s, const String& v) { s.corner_radius_tl = parse_float(v); }},
    {"corner-radius-tr", [](Style& s, const String& v) { s.corner_radius_tr = parse_float(v); }},
    {"corner-radius-br", [](Style& s, const String& v) { s.corner_radius_br = parse_float(v); }},
    {"corner-radius-bl", [](Style& s, const String& v) { s.corner_radius_bl = parse_float(v); }},

    // Visual
    {"opacity", [](Style& s, const String& v) { s.opacity = parse_float(v); }},
    {"aspect-ratio", [](Style& s, const String& v) { s.aspect_ratio = parse_float(v); }},
    {"backdrop-blur", [](Style& s, const String& v) { s.backdrop_blur = parse_float(v); }},

    // Text
    {"font-size", [](Style& s, const String& v) { s.font_size = parse_float(v); }},
    {"letter-spacing", [](Style& s, const String& v) { s.letter_spacing = parse_float(v); }},
    {"line-height", [](Style& s, const String& v) { s.line_height_multiplier = parse_float(v); }},

    // Box shadow
    {"shadow-color", [](Style& s, const String& v) { s.shadow.color = parse_color(v); }},
    {"shadow-blur", [](Style& s, const String& v) { s.shadow.blur = parse_float(v); }},
    {"shadow-spread", [](Style& s, const String& v) { s.shadow.spread = parse_float(v); }},
    {"shadow-x", [](Style& s, const String& v) { s.shadow.offset.x = parse_float(v); }},
    {"shadow-y", [](Style& s, const String& v) { s.shadow.offset.y = parse_float(v); }},
    {"shadow-inset", [](Style& s, const String& v) { s.shadow.inset = (v == "true"); }},

    // Text shadow
    {"text-shadow-color", [](Style& s, const String& v) { s.text_shadow_color = parse_color(v); }},
    {"text-shadow-blur", [](Style& s, const String& v) { s.text_shadow_blur = parse_float(v); }},
    {"text-decoration", [](Style& s, const String& v) {
        if (v == "underline")
            s.text_decoration = TextDecoration::kUnderline;
        else if (v == "line-through" || v == "strikethrough")
            s.text_decoration = TextDecoration::kStrikethrough;
        else if (v == "underline line-through" || v == "underline strikethrough")
            s.text_decoration = TextDecoration::kUnderline | TextDecoration::kStrikethrough;
        else
            s.text_decoration = TextDecoration::kNone;
    }},
    {"text-decoration-color", [](Style& s, const String& v) { s.text_decoration_color = parse_color(v); }},
    {"text-shadow-x", [](Style& s, const String& v) { s.text_shadow_offset.x = parse_float(v); }},
    {"text-shadow-y", [](Style& s, const String& v) { s.text_shadow_offset.y = parse_float(v); }},
};

static StyleSetter FindPropertySetter(std::string_view key) {
    for (auto& [name, setter] : kPropertyTable) {
        if (name == key)
            return setter;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Style mask lookup: maps property names to StyleMask bits
// ---------------------------------------------------------------------------

static constexpr std::pair<std::string_view, u64> kStyleMaskTable[] = {
    {"background", StyleMask::kBackground},
    {"border-color", StyleMask::kBorderColor},
    {"border-width", StyleMask::kBorderWidth},
    {"border-radius", StyleMask::kCornerRadius},
    {"corner-radius", StyleMask::kCornerRadius},
    {"corner-radius-tl", StyleMask::kCornerRadius},
    {"corner-radius-tr", StyleMask::kCornerRadius},
    {"corner-radius-br", StyleMask::kCornerRadius},
    {"corner-radius-bl", StyleMask::kCornerRadius},
    {"opacity", StyleMask::kOpacity},
    {"color", StyleMask::kTextColor},
    {"text-color", StyleMask::kTextColor},
    {"font-size", StyleMask::kFontSize},
    {"width", StyleMask::kWidth},
    {"height", StyleMask::kHeight},
    {"background-end", StyleMask::kBackgroundEnd},
    {"gradient-end", StyleMask::kBackgroundEnd},
    {"gradient-angle", StyleMask::kGradientAngle},
    {"shadow-color", StyleMask::kShadow},
    {"shadow-blur", StyleMask::kShadow},
    {"shadow-spread", StyleMask::kShadow},
    {"shadow-x", StyleMask::kShadow},
    {"shadow-y", StyleMask::kShadow},
    {"shadow-inset", StyleMask::kShadow},
};

static u64 LookupStyleMask(std::string_view key) {
    for (auto& [k, v] : kStyleMaskTable) {
        if (k == key)
            return v;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Transition parsing (pure function)
// ---------------------------------------------------------------------------

static Transition parse_transition_shorthand(const String& val) {
    Transition trans;
    f32 dur = 0;
    const char* p = val.c_str();
    auto r = std::from_chars(p, p + val.size(), dur);
    if (r.ptr != p) {
        trans.duration = (*r.ptr == 'm' && *(r.ptr + 1) == 's') ? dur / 1000.0f : dur;
        p = r.ptr;
        while (*p && (*p == 's' || *p == 'm' || *p == ' '))
            ++p;
    }
    if (auto easing = FindEasingSubstring(p))
        trans.easing = *easing;
    return trans;
}

// ---------------------------------------------------------------------------
// Style parsing: dispatch table driven
// ---------------------------------------------------------------------------

Style UguiBuilder::ParseStyle(const HashMap<String, String>& props) {
    Style s;
    for (auto& [key, val] : props) {
        if (auto setter = FindPropertySetter(key)) {
            String resolved = ResolveValue(val);
            setter(s, resolved);
        }
    }
    return s;
}

// ---------------------------------------------------------------------------
// Widget factory registration
// ---------------------------------------------------------------------------

void UguiBuilder::RegisterType(const String& type_name, WidgetFactory factory) {
    factories_[type_name] = std::move(factory);
}

// ---------------------------------------------------------------------------
// CSS custom properties (--name: value)
// ---------------------------------------------------------------------------

void UguiBuilder::CollectVariables(const UguiNode& node) {
    for (auto& [key, val] : node.properties) {
        if (key.size() > 2 && key[0] == '-' && key[1] == '-')
            variables_[key] = val;
    }
    for (auto& child : node.children)
        CollectVariables(child);
}

String UguiBuilder::ResolveValue(const String& value) const {
    if (value.size() > 2 && value[0] == '-' && value[1] == '-') {
        auto it = variables_.find(value);
        if (it != variables_.end())
            return it->second;
    }
    return value;
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

Widget* UguiBuilder::Build(const UguiDocument& doc) {
    if (doc.roots.empty())
        return nullptr;

    // Collect CSS custom properties from all nodes
    for (auto& root : doc.roots)
        CollectVariables(root);

    u32 id_counter = 1;
    if (doc.roots.size() == 1)
        return BuildNode(doc.roots[0], id_counter);

    // Multiple roots: wrap in a panel
    auto* root = new Panel(0);
    root->set_name("_root");
    for (auto& node : doc.roots) {
        if (auto* child = BuildNode(node, id_counter))
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
    } else if (node.type == "modal" || node.type == "dialog") {
        widget = new Modal(id);
    } else if (node.type == "image" || node.type == "img") {
        widget = new Image(id);
    } else if (node.type == "scroll" || node.type == "scroll-view") {
        widget = new ScrollView(id);
    } else if (node.type == "text-input" || node.type == "input") {
        auto* input = new TextInput(id);
        auto text_it = node.properties.find("placeholder");
        if (text_it != node.properties.end())
            input->set_placeholder(text_it->second);
        auto val_it = node.properties.find("value");
        if (val_it != node.properties.end())
            input->set_text(val_it->second);
        widget = input;
    } else if (node.type == "checkbox") {
        auto* cb = new Checkbox(id);
        auto text_it = node.properties.find("label");
        if (text_it == node.properties.end())
            text_it = node.properties.find("text");
        if (text_it != node.properties.end())
            cb->set_label(text_it->second);
        auto checked_it = node.properties.find("checked");
        if (checked_it != node.properties.end() && checked_it->second == "true")
            cb->set_checked(true);
        widget = cb;
    } else if (node.type == "slider" || node.type == "range") {
        auto* sl = new Slider(id);
        auto min_it = node.properties.find("min");
        if (min_it != node.properties.end())
            sl->set_min(parse_float(min_it->second));
        auto max_it = node.properties.find("max");
        if (max_it != node.properties.end())
            sl->set_max(parse_float(max_it->second));
        auto val_it = node.properties.find("value");
        if (val_it != node.properties.end())
            sl->set_value(parse_float(val_it->second));
        widget = sl;
    } else if (node.type == "radio") {
        auto* radio = new Radio(id);
        auto text_it = node.properties.find("label");
        if (text_it == node.properties.end())
            text_it = node.properties.find("text");
        if (text_it != node.properties.end())
            radio->set_label(text_it->second);
        auto group_it = node.properties.find("group");
        if (group_it != node.properties.end())
            radio->set_group(group_it->second);
        auto checked_it = node.properties.find("checked");
        if (checked_it != node.properties.end() && checked_it->second == "true")
            radio->set_selected(true);
        widget = radio;
    } else if (node.type == "toggle" || node.type == "switch") {
        auto* tog = new Toggle(id);
        auto checked_it = node.properties.find("checked");
        if (checked_it != node.properties.end() && checked_it->second == "true")
            tog->set_on(true);
        widget = tog;
    } else if (node.type == "dropdown" || node.type == "select") {
        auto* dd = new Dropdown(id);
        Vector<String> opts;
        for (auto& child_node : node.children) {
            auto text_it = child_node.properties.find("text");
            if (text_it == child_node.properties.end())
                text_it = child_node.properties.find("label");
            if (text_it != child_node.properties.end())
                opts.push_back(text_it->second);
            else if (!child_node.name.empty())
                opts.push_back(child_node.name);
        }
        dd->set_options(opts);
        auto sel_it = node.properties.find("selected");
        if (sel_it != node.properties.end())
            dd->set_selected_index(static_cast<i32>(parse_float(sel_it->second)));
        widget = dd;
        // Return early: option children are data, not child widgets
        widget->set_id(id);
        widget->set_name(node.name);
        ApplyProperties(widget, node);
        return widget;
    } else if (node.type == "context-menu") {
        auto* menu = new ContextMenu(id);
        for (auto& child_node : node.children) {
            if (child_node.type == "separator") {
                menu->AddSeparator();
            } else {
                auto text_it = child_node.properties.find("text");
                if (text_it == child_node.properties.end())
                    text_it = child_node.properties.find("label");
                String label = text_it != child_node.properties.end()
                                        ? text_it->second
                                        : child_node.name;
                menu->AddItem(label, nullptr);
            }
        }
        widget = menu;
        // Return early: item children are data, not child widgets
        widget->set_id(id);
        widget->set_name(node.name);
        ApplyProperties(widget, node);
        return widget;
    } else if (node.type == "rich-text" || node.type == "richtext") {
        auto* rt = new RichText(id);
        // Parse child "span" elements as TextSpan data, not child widgets
        for (const auto& child_node : node.children) {
            if (child_node.type == "span") {
                TextSpan span;
                auto text_it = child_node.properties.find("text");
                if (text_it == child_node.properties.end())
                    text_it = child_node.properties.find("content");
                if (text_it != child_node.properties.end())
                    span.text = text_it->second;

                auto color_it = child_node.properties.find("color");
                if (color_it != child_node.properties.end())
                    span.color = parse_color(color_it->second);

                auto size_it = child_node.properties.find("font-size");
                if (size_it != child_node.properties.end())
                    span.font_size = parse_float(size_it->second);

                auto weight_it = child_node.properties.find("font-weight");
                if (weight_it != child_node.properties.end()) {
                    static constexpr std::pair<std::string_view, FontWeight>
                        kSpanWeightTable[] = {
                            {"thin", FontWeight::kThin},
                            {"extra-light", FontWeight::kExtraLight},
                            {"light", FontWeight::kLight},
                            {"regular", FontWeight::kRegular},
                            {"normal", FontWeight::kRegular},
                            {"medium", FontWeight::kMedium},
                            {"semi-bold", FontWeight::kSemiBold},
                            {"semibold", FontWeight::kSemiBold},
                            {"bold", FontWeight::kBold},
                            {"extra-bold", FontWeight::kExtraBold},
                            {"black", FontWeight::kBlack},
                        };
                    if (auto w = LookupEnum(kSpanWeightTable, weight_it->second))
                        span.font_weight = *w;
                }

                auto style_it = child_node.properties.find("font-style");
                if (style_it != child_node.properties.end() &&
                    style_it->second == "italic") {
                    span.font_style = FontStyle::kItalic;
                }

                auto dec_it = child_node.properties.find("text-decoration");
                if (dec_it != child_node.properties.end()) {
                    if (dec_it->second == "underline")
                        span.decoration = TextDecoration::kUnderline;
                    else if (dec_it->second == "line-through" ||
                             dec_it->second == "strikethrough")
                        span.decoration = TextDecoration::kStrikethrough;
                    else if (dec_it->second == "underline line-through" ||
                             dec_it->second == "underline strikethrough")
                        span.decoration = TextDecoration::kUnderline |
                                          TextDecoration::kStrikethrough;
                }

                rt->AddSpan(span);
            }
        }
        widget = rt;
        // Return early: span children are data, not child widgets
        widget->set_id(id);
        widget->set_name(node.name);
        ApplyProperties(widget, node);
        return widget;
    } else {
        // Unknown type: treat as panel
        std::fprintf(stderr, "ultragui: unknown element type '%s' at line %u\n", node.type.c_str(),
                     node.source_line);
        widget = new Panel(id);
    }

    widget->set_id(id);
    widget->set_name(node.name);

    ApplyProperties(widget, node);

    for (auto& child_node : node.children) {
        if (auto* child = BuildNode(child_node, id_counter))
            widget->AddChild(child);
    }

    return widget;
}

void UguiBuilder::ApplyProperties(Widget* widget, const UguiNode& node) {
    widget->set_style(ParseStyle(node.properties));

    // Tooltip
    auto tooltip_it = node.properties.find("tooltip");
    if (tooltip_it != node.properties.end())
        widget->set_tooltip(tooltip_it->second);

    // Tab navigation
    auto tab_it = node.properties.find("tab-index");
    if (tab_it != node.properties.end())
        widget->set_tab_index(static_cast<i32>(parse_float(tab_it->second)));

    // State overrides
    for (auto& sb : node.state_blocks) {
        WidgetState state = LookupEnum(kWidgetStateTable, sb.state).value_or(WidgetState::kNone);
        Style override_style = ParseStyle(sb.properties);

        // Build mask via fold over property keys
        u64 mask = std::accumulate(sb.properties.begin(), sb.properties.end(), u64{0},
                                   [](u64 acc, const auto& kv) {
                                       return acc | LookupStyleMask(kv.first);
                                   });

        widget->AddStateOverride(state, override_style, mask);

        // Parse transition
        Transition trans;
        bool has_transition = false;

        if (auto t_it = sb.properties.find("transition"); t_it != sb.properties.end()) {
            trans = parse_transition_shorthand(t_it->second);
            has_transition = true;
        }
        if (auto td_it = sb.properties.find("transition-duration"); td_it != sb.properties.end()) {
            trans.duration = parse_duration(td_it->second);
            has_transition = true;
        }
        if (auto te_it = sb.properties.find("transition-easing"); te_it != sb.properties.end()) {
            if (auto e = LookupEnum(kEasingTable, te_it->second))
                trans.easing = *e;
        }
        if (auto tl_it = sb.properties.find("transition-delay"); tl_it != sb.properties.end()) {
            trans.delay = parse_duration(tl_it->second);
        }
        if (auto ss_it = sb.properties.find("spring-stiffness"); ss_it != sb.properties.end())
            trans.spring_stiffness = parse_float(ss_it->second);
        if (auto sd_it = sb.properties.find("spring-damping"); sd_it != sb.properties.end())
            trans.spring_damping = parse_float(sd_it->second);
        if (auto sm_it = sb.properties.find("spring-mass"); sm_it != sb.properties.end())
            trans.spring_mass = parse_float(sm_it->second);

        if (has_transition && state != WidgetState::kNone)
            widget->AddStateTransition(state, trans);
    }

    // Media query overrides: apply matching conditions on top of the base style
    for (auto& mq : node.media_queries) {
        bool matches = false;
        if (mq.condition == "min-width")
            matches = viewport_size_.x >= mq.value;
        else if (mq.condition == "max-width")
            matches = viewport_size_.x <= mq.value;
        else if (mq.condition == "min-height")
            matches = viewport_size_.y >= mq.value;
        else if (mq.condition == "max-height")
            matches = viewport_size_.y <= mq.value;

        if (matches) {
            Style& style = widget->style();
            for (auto& [key, val] : mq.properties) {
                if (auto setter = FindPropertySetter(key)) {
                    String resolved = ResolveValue(val);
                    setter(style, resolved);
                }
            }
        }
    }

    // @keyframes blocks
    for (auto& kb : node.keyframe_blocks) {
        if (!animator_ || kb.stops.empty())
            continue;

        KeyframeAnimation anim;
        anim.widget_id = widget->id();

        // Parse top-level properties
        if (auto d_it = kb.properties.find("duration"); d_it != kb.properties.end())
            anim.duration = parse_duration(d_it->second);
        if (auto l_it = kb.properties.find("loop"); l_it != kb.properties.end())
            anim.repeat_count = (l_it->second == "true") ? -1 : 1;
        if (auto a_it = kb.properties.find("alternate"); a_it != kb.properties.end())
            anim.alternate = (a_it->second == "true");

        // Transform stops -> sorted keyframes
        anim.keyframes.reserve(kb.stops.size());
        std::transform(kb.stops.begin(), kb.stops.end(), std::back_inserter(anim.keyframes),
                       [this](const auto& stop) {
                           return Keyframe{stop.percent, ParseStyle(stop.properties)};
                       });
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
