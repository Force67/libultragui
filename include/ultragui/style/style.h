#ifndef ULTRAGUI_STYLE_STYLE_H_
#define ULTRAGUI_STYLE_STYLE_H_

#include <ultragui/core/color.h>
#include <ultragui/core/rect.h>
#include <ultragui/style/enums.h>
#include <ultragui/style/property_types.h>
#include <ultragui/style/transition.h>

namespace ugui {

/// A single color stop in a multi-stop gradient.
struct GradientStop {
    f32 position = 0.0f;  // 0.0 to 1.0
    Color color;
};

/// Box shadow definition (CSS-like).
struct BoxShadow {
    Color color = Color::Transparent();
    f32 blur = 0.0f;
    f32 spread = 0.0f;
    Vec2 offset = Vec2::Zero();
    bool inset = false;
};

/// Complete visual style for a widget. Every widget has one of these.
/// Properties use CSS-like naming. Default values produce an invisible,
/// auto-sized, flow-layout element (like an unstyled <div>).
struct Style {
    // --- Layout ---
    FlexDirection flex_direction = FlexDirection::kRow;
    JustifyContent justify_content = JustifyContent::kStart;
    AlignItems align_items = AlignItems::kStretch;
    AlignSelf align_self = AlignSelf::kAuto;
    FlexWrap flex_wrap = FlexWrap::kNoWrap;
    Position position = Position::kRelative;
    Overflow overflow = Overflow::kVisible;
    Visibility visibility = Visibility::kVisible;

    // --- Sizing ---
    Length width = Length::Auto();
    Length height = Length::Auto();
    Length min_width = Length::Px(0);
    Length min_height = Length::Px(0);
    Length max_width = Length::Px(1e6f); // effectively unbounded
    Length max_height = Length::Px(1e6f);
    f32 flex_grow = 0.0f;
    f32 flex_shrink = 1.0f;

    // --- Spacing ---
    EdgeInsets margin;
    EdgeInsets padding;
    f32 gap = 0.0f;

    // --- Position offsets (for Position::kAbsolute or Relative offsets) ---
    Length top = Length::Auto();
    Length right_offset = Length::Auto();
    Length bottom = Length::Auto();
    Length left_offset = Length::Auto();

    // --- Visual ---
    Color background = Color::Transparent();
    Color background_end = Color::Transparent(); // If != transparent, linear gradient top->bottom
    f32 gradient_angle = 180.0f;                 // CSS degrees: 180 = top-to-bottom (default)
    GradientType gradient_type = GradientType::kLinear;
    static constexpr u32 kMaxGradientStops = 8;
    GradientStop gradient_stops[kMaxGradientStops] = {};
    u32 gradient_stop_count = 0;
    Color border_color = Color::Transparent();
    f32 border_width = 0.0f;
    f32 corner_radius = 0.0f;    // Convenience: sets all 4 corners
    f32 corner_radius_tl = 0.0f; // Per-corner: top-left
    f32 corner_radius_tr = 0.0f; // Per-corner: top-right
    f32 corner_radius_br = 0.0f; // Per-corner: bottom-right
    f32 corner_radius_bl = 0.0f; // Per-corner: bottom-left
    f32 opacity = 1.0f;
    f32 backdrop_blur = 0.0f;    // Blur radius for frosted glass effect (0 = none)
    f32 aspect_ratio = 0.0f;     // 0 = none, positive = width/height

    // --- Box shadow ---
    BoxShadow shadow;

    // --- Text ---
    Color text_color = Color::White();
    f32 font_size = 16.0f;
    TextAlign text_align = TextAlign::kLeft;
    f32 letter_spacing = 0.0f;         // Extra pixels between characters
    f32 line_height_multiplier = 1.0f; // Multiplier on default line height
    TextTransform text_transform = TextTransform::kNone;
    FontWeight font_weight = FontWeight::kRegular;
    FontStyle font_style = FontStyle::kNormal;
    TextDecoration text_decoration = TextDecoration::kNone;
    Color text_decoration_color = Color::Transparent(); // transparent = inherit text_color

    // --- Text shadow ---
    Color text_shadow_color = Color::Transparent();
    f32 text_shadow_blur = 0.0f;
    Vec2 text_shadow_offset = Vec2::Zero();

    // --- Cursor ---
    Cursor cursor = Cursor::kAuto;

    // --- Transitions (keyed by property group) ---
    Transition background_transition;
    Transition transform_transition;
    Transition opacity_transition;

    /// Check if this style has a gradient background
    bool HasGradient() const {
        return background_end.a > 0.0f && background_end != background;
    }

    /// Check if this style has a multi-stop gradient (3+ colors)
    bool HasMultiStopGradient() const { return gradient_stop_count >= 2; }

    /// Check if this style has a box shadow
    bool HasShadow() const {
        return shadow.color.a > 0.0f && (shadow.blur > 0.0f || shadow.spread > 0.0f);
    }

    /// Scale all pixel-valued visual properties by a factor.
    /// Used for viewport-responsive scaling. Layout properties (padding, margin,
    /// gap, dimensions) are NOT scaled here - the layout engine handles those.
    /// This scales: font_size, letter_spacing, border_width, corner_radius,
    /// shadow, text_shadow, and backdrop_blur.
    void Scale(f32 s);

    /// Linearly interpolate between two styles for animation.
    /// Only interpolates animatable properties (colors, sizes, opacity, etc).
    static Style Lerp(const Style& a, const Style& b, f32 t);
};

/// A style override for a specific widget state (e.g. :hover, :pressed).
/// Only the fields that are explicitly set differ from the base style.
/// The `mask` indicates which properties are overridden.
struct StyleOverride {
    WidgetState state = WidgetState::kNone;
    Style style;  // Only masked properties are used
    u64 mask = 0; // Bitmask of which properties are overridden
};

// Property mask bits for StyleOverride
namespace StyleMask {
constexpr u64 kBackground = 1ull << 0;
constexpr u64 kBorderColor = 1ull << 1;
constexpr u64 kBorderWidth = 1ull << 2;
constexpr u64 kCornerRadius = 1ull << 3;
constexpr u64 kOpacity = 1ull << 4;
constexpr u64 kTextColor = 1ull << 5;
constexpr u64 kFontSize = 1ull << 6;
constexpr u64 kWidth = 1ull << 7;
constexpr u64 kHeight = 1ull << 8;
constexpr u64 kMargin = 1ull << 9;
constexpr u64 kPadding = 1ull << 10;
constexpr u64 kTransform = 1ull << 11;
constexpr u64 kShadow = 1ull << 12;
constexpr u64 kBackgroundEnd = 1ull << 13;
constexpr u64 kGradientAngle = 1ull << 14;
} // namespace StyleMask

/// Resolve the effective style for a widget given its base style,
/// state overrides, and current interactive state.
Style ResolveStyle(const Style& base, const StyleOverride* overrides, u32 override_count,
                    WidgetState current_state);

} // namespace ugui

#endif  // ULTRAGUI_STYLE_STYLE_H_
