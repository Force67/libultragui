#include <ultragui/core/math.h>
#include <ultragui/style/style.h>

namespace ugui {

static Length lerp_length(const Length& a, const Length& b, f32 t) {
    if (a.unit != b.unit)
        return t < 0.5f ? a : b;
    return Length(ugui::Lerp(a.value, b.value, t), a.unit);
}

static EdgeInsets lerp_insets(const EdgeInsets& a, const EdgeInsets& b, f32 t) {
    return {
        ugui::Lerp(a.top, b.top, t),
        ugui::Lerp(a.right, b.right, t),
        ugui::Lerp(a.bottom, b.bottom, t),
        ugui::Lerp(a.left, b.left, t),
    };
}

static BoxShadow lerp_shadow(const BoxShadow& a, const BoxShadow& b, f32 t) {
    return {
        ugui::Lerp(a.color, b.color, t),
        ugui::Lerp(a.blur, b.blur, t),
        ugui::Lerp(a.spread, b.spread, t),
        ugui::Lerp(a.offset, b.offset, t),
    };
}

Style Style::Lerp(const Style& a, const Style& b, f32 t) {
    Style result = a; // Start with 'a', interpolate animatable fields

    result.background = ugui::Lerp(a.background, b.background, t);
    result.background_end = ugui::Lerp(a.background_end, b.background_end, t);
    result.border_color = ugui::Lerp(a.border_color, b.border_color, t);
    result.border_width = ugui::Lerp(a.border_width, b.border_width, t);
    result.corner_radius = ugui::Lerp(a.corner_radius, b.corner_radius, t);
    result.corner_radius_tl = ugui::Lerp(a.corner_radius_tl, b.corner_radius_tl, t);
    result.corner_radius_tr = ugui::Lerp(a.corner_radius_tr, b.corner_radius_tr, t);
    result.corner_radius_br = ugui::Lerp(a.corner_radius_br, b.corner_radius_br, t);
    result.corner_radius_bl = ugui::Lerp(a.corner_radius_bl, b.corner_radius_bl, t);
    result.opacity = ugui::Lerp(a.opacity, b.opacity, t);
    result.aspect_ratio = ugui::Lerp(a.aspect_ratio, b.aspect_ratio, t);
    result.text_color = ugui::Lerp(a.text_color, b.text_color, t);
    result.font_size = ugui::Lerp(a.font_size, b.font_size, t);
    result.gap = ugui::Lerp(a.gap, b.gap, t);
    result.letter_spacing = ugui::Lerp(a.letter_spacing, b.letter_spacing, t);
    result.line_height_multiplier = ugui::Lerp(a.line_height_multiplier, b.line_height_multiplier, t);
    result.text_shadow_color = ugui::Lerp(a.text_shadow_color, b.text_shadow_color, t);
    result.text_shadow_blur = ugui::Lerp(a.text_shadow_blur, b.text_shadow_blur, t);
    result.text_shadow_offset = ugui::Lerp(a.text_shadow_offset, b.text_shadow_offset, t);

    result.shadow = lerp_shadow(a.shadow, b.shadow, t);

    result.width = lerp_length(a.width, b.width, t);
    result.height = lerp_length(a.height, b.height, t);
    result.margin = lerp_insets(a.margin, b.margin, t);
    result.padding = lerp_insets(a.padding, b.padding, t);

    // Non-animatable properties snap at t >= 0.5
    if (t >= 0.5f) {
        result.flex_direction = b.flex_direction;
        result.justify_content = b.justify_content;
        result.align_items = b.align_items;
        result.align_self = b.align_self;
        result.flex_wrap = b.flex_wrap;
        result.position = b.position;
        result.overflow = b.overflow;
        result.visibility = b.visibility;
        result.text_align = b.text_align;
    }

    return result;
}

Style ResolveStyle(const Style& base, const StyleOverride* overrides, u32 override_count,
                    WidgetState current_state) {
    Style result = base;

    for (u32 i = 0; i < override_count; ++i) {
        const auto& ov = overrides[i];
        // Check if the override's state is active
        if (ov.state != WidgetState::kNone && !HasState(current_state, ov.state))
            continue;

        // Apply masked properties
        if (ov.mask & StyleMask::kBackground)
            result.background = ov.style.background;
        if (ov.mask & StyleMask::kBackgroundEnd)
            result.background_end = ov.style.background_end;
        if (ov.mask & StyleMask::kBorderColor)
            result.border_color = ov.style.border_color;
        if (ov.mask & StyleMask::kBorderWidth)
            result.border_width = ov.style.border_width;
        if (ov.mask & StyleMask::kCornerRadius) {
            result.corner_radius = ov.style.corner_radius;
            result.corner_radius_tl = ov.style.corner_radius_tl;
            result.corner_radius_tr = ov.style.corner_radius_tr;
            result.corner_radius_br = ov.style.corner_radius_br;
            result.corner_radius_bl = ov.style.corner_radius_bl;
        }
        if (ov.mask & StyleMask::kOpacity)
            result.opacity = ov.style.opacity;
        if (ov.mask & StyleMask::kTextColor)
            result.text_color = ov.style.text_color;
        if (ov.mask & StyleMask::kFontSize)
            result.font_size = ov.style.font_size;
        if (ov.mask & StyleMask::kWidth)
            result.width = ov.style.width;
        if (ov.mask & StyleMask::kHeight)
            result.height = ov.style.height;
        if (ov.mask & StyleMask::kMargin)
            result.margin = ov.style.margin;
        if (ov.mask & StyleMask::kPadding)
            result.padding = ov.style.padding;
        if (ov.mask & StyleMask::kShadow)
            result.shadow = ov.style.shadow;
    }

    return result;
}

} // namespace ugui
