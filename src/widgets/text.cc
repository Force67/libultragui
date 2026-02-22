#include <ultragui/render/renderer2d.h>
#include <ultragui/widgets/text.h>

#include <cctype>
#include <string>

namespace ugui {

static String apply_transform(const String& s, TextTransform t) {
    if (t == TextTransform::kNone) return s;
    String out = s;
    if (t == TextTransform::kUppercase) {
        for (auto& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    } else if (t == TextTransform::kLowercase) {
        for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    } else if (t == TextTransform::kCapitalize) {
        bool next = true;
        for (auto& c : out) {
            if (next && std::isalpha(static_cast<unsigned char>(c))) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                next = false;
            }
            if (c == ' ') next = true;
        }
    }
    return out;
}

void Text::Measure(f32& out_width, f32& out_height) {
    auto* te = text_engine();
    FontHandle fh = effective_font();
    if (!te || fh == kInvalidFont || text_.empty()) {
        out_width = 0;
        out_height = 0;
        return;
    }

    FontHandle resolved = fh;
    if (style_.font_weight != FontWeight::kRegular || style_.font_style != FontStyle::kNormal)
        resolved = te->ResolveFont(fh, style_.font_weight, style_.font_style);

    String display_text = apply_transform(text_, style_.text_transform);
    f32 sc = ui_scale();
    auto run =
        te->Shape(resolved, display_text.c_str(), static_cast<u32>(display_text.size()),
                  style_.font_size * sc, style_.letter_spacing * sc, style_.line_height_multiplier);
    out_width = run.total_advance;
    out_height = run.line_height;
}

void Text::OnPaint(Renderer2D& renderer) {
    Widget::OnPaint(renderer); // Background, shadow, border

    auto* te = text_engine();
    FontHandle fh = effective_font();
    if (!te || fh == kInvalidFont || text_.empty())
        return;

    auto s = ComputedStyle();
    s.Scale(ui_scale());
    f32 alpha = s.opacity;

    // Resolve font for weight/style
    FontHandle resolved = fh;
    if (s.font_weight != FontWeight::kRegular || s.font_style != FontStyle::kNormal)
        resolved = te->ResolveFont(fh, s.font_weight, s.font_style);

    // Apply text transform
    String display_text = apply_transform(text_, s.text_transform);

    // Always shape fresh - the scratch buffer pointer from measure may be stale
    auto run =
        te->Shape(resolved, display_text.c_str(), static_cast<u32>(display_text.size()),
                  s.font_size, s.letter_spacing, s.line_height_multiplier);

    // Position text within content rect
    f32 x = content_rect_.x;
    f32 y = content_rect_.y;

    // Horizontal alignment
    switch (s.text_align) {
    case TextAlign::kCenter:
        x += (content_rect_.w - run.total_advance) * 0.5f;
        break;
    case TextAlign::kRight:
        x += content_rect_.w - run.total_advance;
        break;
    default:
        break;
    }

    // Vertical center
    y += (content_rect_.h - run.line_height) * 0.5f;

    Color text_color = s.text_color.WithAlpha(s.text_color.a * alpha);

    // Text shadow (drawn first, behind the main text)
    if (s.text_shadow_color.a > 0.0f) {
        Vec2 shadow_pos = {x + s.text_shadow_offset.x, y + s.text_shadow_offset.y};
        Color shadow_color = s.text_shadow_color.WithAlpha(s.text_shadow_color.a * alpha);
        renderer.DrawText(shadow_pos, run, shadow_color, te->atlas_texture());
    }

    renderer.DrawText(Vec2{x, y}, run, text_color, te->atlas_texture());

    // Text decoration (underline / strikethrough)
    if (s.text_decoration != TextDecoration::kNone) {
        Color dec_color = s.text_decoration_color.a > 0.0f
                              ? s.text_decoration_color.WithAlpha(s.text_decoration_color.a * alpha)
                              : text_color;
        f32 thickness = std::max(1.0f, s.font_size / 14.0f);
        f32 baseline = y + run.ascent;

        if (HasDecoration(s.text_decoration, TextDecoration::kUnderline)) {
            f32 line_y = baseline + s.font_size * 0.15f;
            renderer.DrawRect({x, line_y, run.total_advance, thickness}, dec_color);
        }
        if (HasDecoration(s.text_decoration, TextDecoration::kStrikethrough)) {
            f32 line_y = baseline - s.font_size * 0.3f;
            renderer.DrawRect({x, line_y, run.total_advance, thickness}, dec_color);
        }
    }
}

} // namespace ugui
