#include <ugui/core/math.h>
#include <ugui/style/style.h>

#include <span>

namespace ugui {

static Length lerp_length(const Length& a, const Length& b, f32 t) {
  if (a.unit != b.unit) return t < 0.5f ? a : b;
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
      ugui::Lerp(a.color, b.color, t),   ugui::Lerp(a.blur, b.blur, t),
      ugui::Lerp(a.spread, b.spread, t), ugui::Lerp(a.offset, b.offset, t),
      t < 0.5f ? a.inset : b.inset,
  };
}

void Style::Scale(f32 s) {
  if (s == 1.0f) return;
  font_size *= s;
  letter_spacing *= s;
  border_width *= s;
  corner_radius *= s;
  corner_radius_tl *= s;
  corner_radius_tr *= s;
  corner_radius_br *= s;
  corner_radius_bl *= s;
  shadow.blur *= s;
  shadow.spread *= s;
  shadow.offset.x *= s;
  shadow.offset.y *= s;
  text_shadow_blur *= s;
  text_shadow_offset.x *= s;
  text_shadow_offset.y *= s;
  backdrop_blur *= s;
}

Style Style::Lerp(const Style& a, const Style& b, f32 t) {
  Style result = a;  // Start with 'a', interpolate animatable fields

  result.background = ugui::Lerp(a.background, b.background, t);
  result.background_end = ugui::Lerp(a.background_end, b.background_end, t);
  result.gradient_angle = ugui::Lerp(a.gradient_angle, b.gradient_angle, t);
  result.border_color = ugui::Lerp(a.border_color, b.border_color, t);
  result.border_width = ugui::Lerp(a.border_width, b.border_width, t);
  result.corner_radius = ugui::Lerp(a.corner_radius, b.corner_radius, t);
  result.corner_radius_tl =
      ugui::Lerp(a.corner_radius_tl, b.corner_radius_tl, t);
  result.corner_radius_tr =
      ugui::Lerp(a.corner_radius_tr, b.corner_radius_tr, t);
  result.corner_radius_br =
      ugui::Lerp(a.corner_radius_br, b.corner_radius_br, t);
  result.corner_radius_bl =
      ugui::Lerp(a.corner_radius_bl, b.corner_radius_bl, t);
  result.opacity = ugui::Lerp(a.opacity, b.opacity, t);
  result.backdrop_blur = ugui::Lerp(a.backdrop_blur, b.backdrop_blur, t);
  result.aspect_ratio = ugui::Lerp(a.aspect_ratio, b.aspect_ratio, t);
  result.text_color = ugui::Lerp(a.text_color, b.text_color, t);
  result.font_size = ugui::Lerp(a.font_size, b.font_size, t);
  result.gap = ugui::Lerp(a.gap, b.gap, t);
  result.letter_spacing = ugui::Lerp(a.letter_spacing, b.letter_spacing, t);
  result.line_height_multiplier =
      ugui::Lerp(a.line_height_multiplier, b.line_height_multiplier, t);
  result.text_shadow_color =
      ugui::Lerp(a.text_shadow_color, b.text_shadow_color, t);
  result.text_shadow_blur =
      ugui::Lerp(a.text_shadow_blur, b.text_shadow_blur, t);
  result.text_shadow_offset =
      ugui::Lerp(a.text_shadow_offset, b.text_shadow_offset, t);
  result.text_decoration_color =
      ugui::Lerp(a.text_decoration_color, b.text_decoration_color, t);

  result.shadow = lerp_shadow(a.shadow, b.shadow, t);

  // Gradient stops
  result.gradient_stop_count =
      std::max(a.gradient_stop_count, b.gradient_stop_count);
  for (u32 i = 0; i < result.gradient_stop_count; ++i) {
    const auto& sa = (i < a.gradient_stop_count)
                         ? a.gradient_stops[i]
                         : a.gradient_stops[a.gradient_stop_count > 0
                                                ? a.gradient_stop_count - 1
                                                : 0];
    const auto& sb = (i < b.gradient_stop_count)
                         ? b.gradient_stops[i]
                         : b.gradient_stops[b.gradient_stop_count > 0
                                                ? b.gradient_stop_count - 1
                                                : 0];
    result.gradient_stops[i].position = ugui::Lerp(sa.position, sb.position, t);
    result.gradient_stops[i].color = ugui::Lerp(sa.color, sb.color, t);
  }

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
    result.font_weight = b.font_weight;
    result.font_style = b.font_style;
    result.gradient_type = b.gradient_type;
  }

  return result;
}

// ---------------------------------------------------------------------------
// Table-driven style override resolution
// ---------------------------------------------------------------------------

using MaskApply = void (*)(Style&, const Style&);

static const std::pair<u64, MaskApply> kMaskApplicators[] = {
    {StyleMask::kBackground,
     [](Style& r, const Style& s) { r.background = s.background; }},
    {StyleMask::kBackgroundEnd,
     [](Style& r, const Style& s) { r.background_end = s.background_end; }},
    {StyleMask::kBorderColor,
     [](Style& r, const Style& s) { r.border_color = s.border_color; }},
    {StyleMask::kBorderWidth,
     [](Style& r, const Style& s) { r.border_width = s.border_width; }},
    {StyleMask::kCornerRadius,
     [](Style& r, const Style& s) {
       r.corner_radius = s.corner_radius;
       r.corner_radius_tl = s.corner_radius_tl;
       r.corner_radius_tr = s.corner_radius_tr;
       r.corner_radius_br = s.corner_radius_br;
       r.corner_radius_bl = s.corner_radius_bl;
     }},
    {StyleMask::kOpacity,
     [](Style& r, const Style& s) { r.opacity = s.opacity; }},
    {StyleMask::kTextColor,
     [](Style& r, const Style& s) { r.text_color = s.text_color; }},
    {StyleMask::kFontSize,
     [](Style& r, const Style& s) { r.font_size = s.font_size; }},
    {StyleMask::kWidth, [](Style& r, const Style& s) { r.width = s.width; }},
    {StyleMask::kHeight, [](Style& r, const Style& s) { r.height = s.height; }},
    {StyleMask::kMargin, [](Style& r, const Style& s) { r.margin = s.margin; }},
    {StyleMask::kPadding,
     [](Style& r, const Style& s) { r.padding = s.padding; }},
    {StyleMask::kShadow, [](Style& r, const Style& s) { r.shadow = s.shadow; }},
    {StyleMask::kGradientAngle,
     [](Style& r, const Style& s) { r.gradient_angle = s.gradient_angle; }},
};

static void ApplyMaskedOverride(Style& result, const StyleOverride& ov) {
  for (auto& [mask, apply] : kMaskApplicators) {
    if (ov.mask & mask) apply(result, ov.style);
  }
}

Style ResolveStyle(const Style& base, const StyleOverride* overrides,
                   u32 override_count, WidgetState current_state) {
  Style result = base;
  auto is_active = [current_state](const StyleOverride& ov) {
    return ov.state == WidgetState::kNone || HasState(current_state, ov.state);
  };
  for (auto& ov : std::span(overrides, override_count)) {
    if (is_active(ov)) ApplyMaskedOverride(result, ov);
  }
  return result;
}

}  // namespace ugui
