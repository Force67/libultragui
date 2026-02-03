#pragma once

#include <ultragui/core/color.h>
#include <ultragui/core/rect.h>
#include <ultragui/style/enums.h>
#include <ultragui/style/property_types.h>
#include <ultragui/style/transition.h>

namespace ugui {

/// Box shadow definition (CSS-like).
struct BoxShadow {
    Color color = Color::transparent();
    f32 blur = 0.0f;
    f32 spread = 0.0f;
    Vec2 offset = Vec2::zero();
};

/// Complete visual style for a widget. Every widget has one of these.
/// Properties use CSS-like naming. Default values produce an invisible,
/// auto-sized, flow-layout element (like an unstyled <div>).
struct Style {
    // --- Layout ---
    FlexDirection flex_direction = FlexDirection::Row;
    JustifyContent justify_content = JustifyContent::Start;
    AlignItems align_items = AlignItems::Stretch;
    AlignSelf align_self = AlignSelf::Auto;
    FlexWrap flex_wrap = FlexWrap::NoWrap;
    Position position = Position::Relative;
    Overflow overflow = Overflow::Visible;
    Visibility visibility = Visibility::Visible;

    // --- Sizing ---
    Length width = Length::auto_();
    Length height = Length::auto_();
    Length min_width = Length::px(0);
    Length min_height = Length::px(0);
    Length max_width = Length::px(1e6f); // effectively unbounded
    Length max_height = Length::px(1e6f);
    f32 flex_grow = 0.0f;
    f32 flex_shrink = 1.0f;

    // --- Spacing ---
    EdgeInsets margin;
    EdgeInsets padding;
    f32 gap = 0.0f;

    // --- Position offsets (for Position::Absolute or Relative offsets) ---
    Length top = Length::auto_();
    Length right_offset = Length::auto_();
    Length bottom = Length::auto_();
    Length left_offset = Length::auto_();

    // --- Visual ---
    Color background = Color::transparent();
    Color background_end = Color::transparent(); // If != transparent, linear gradient top->bottom
    Color border_color = Color::transparent();
    f32 border_width = 0.0f;
    f32 corner_radius = 0.0f;    // Convenience: sets all 4 corners
    f32 corner_radius_tl = 0.0f; // Per-corner: top-left
    f32 corner_radius_tr = 0.0f; // Per-corner: top-right
    f32 corner_radius_br = 0.0f; // Per-corner: bottom-right
    f32 corner_radius_bl = 0.0f; // Per-corner: bottom-left
    f32 opacity = 1.0f;
    f32 aspect_ratio = 0.0f;     // 0 = none, positive = width/height

    // --- Box shadow ---
    BoxShadow shadow;

    // --- Text ---
    Color text_color = Color::white();
    f32 font_size = 16.0f;
    TextAlign text_align = TextAlign::Left;
    f32 letter_spacing = 0.0f;         // Extra pixels between characters
    f32 line_height_multiplier = 1.0f; // Multiplier on default line height
    TextTransform text_transform = TextTransform::None;

    // --- Text shadow ---
    Color text_shadow_color = Color::transparent();
    f32 text_shadow_blur = 0.0f;
    Vec2 text_shadow_offset = Vec2::zero();

    // --- Cursor ---
    Cursor cursor = Cursor::Auto;

    // --- Transitions (keyed by property group) ---
    Transition background_transition;
    Transition transform_transition;
    Transition opacity_transition;

    /// Check if this style has a gradient background
    bool has_gradient() const {
        return background_end.a > 0.0f && background_end != background;
    }

    /// Check if this style has a box shadow
    bool has_shadow() const {
        return shadow.color.a > 0.0f && (shadow.blur > 0.0f || shadow.spread > 0.0f);
    }

    /// Linearly interpolate between two styles for animation.
    /// Only interpolates animatable properties (colors, sizes, opacity, etc).
    static Style lerp(const Style& a, const Style& b, f32 t);
};

/// A style override for a specific widget state (e.g. :hover, :pressed).
/// Only the fields that are explicitly set differ from the base style.
/// The `mask` indicates which properties are overridden.
struct StyleOverride {
    WidgetState state = WidgetState::None;
    Style style;  // Only masked properties are used
    u64 mask = 0; // Bitmask of which properties are overridden
};

// Property mask bits for StyleOverride
namespace StyleMask {
constexpr u64 Background = 1ull << 0;
constexpr u64 BorderColor = 1ull << 1;
constexpr u64 BorderWidth = 1ull << 2;
constexpr u64 CornerRadius = 1ull << 3;
constexpr u64 Opacity = 1ull << 4;
constexpr u64 TextColor = 1ull << 5;
constexpr u64 FontSize = 1ull << 6;
constexpr u64 Width = 1ull << 7;
constexpr u64 Height = 1ull << 8;
constexpr u64 Margin = 1ull << 9;
constexpr u64 Padding = 1ull << 10;
constexpr u64 Transform = 1ull << 11;
constexpr u64 Shadow = 1ull << 12;
constexpr u64 BackgroundEnd = 1ull << 13;
} // namespace StyleMask

/// Resolve the effective style for a widget given its base style,
/// state overrides, and current interactive state.
Style resolve_style(const Style& base, const StyleOverride* overrides, u32 override_count,
                    WidgetState current_state);

} // namespace ugui
