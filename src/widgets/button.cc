#include <ultragui/render/renderer2d.h>
#include <ultragui/text/text_engine.h>
#include <ultragui/widgets/button.h>

#include <cctype>
#include <string>

namespace ugui {

static std::string apply_transform(const std::string& s, TextTransform t) {
    if (t == TextTransform::kNone) return s;
    std::string out = s;
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

void Button::Measure(f32& out_width, f32& out_height) {
    auto* te = text_engine();
    FontHandle fh = effective_font();
    if (!te || fh == kInvalidFont || label_.empty()) {
        out_width = 0;
        out_height = style_.font_size + style_.padding.vertical();
        return;
    }

    FontHandle resolved = fh;
    if (style_.font_weight != FontWeight::kRegular || style_.font_style != FontStyle::kNormal)
        resolved = te->ResolveFont(fh, style_.font_weight, style_.font_style);

    std::string display_label = apply_transform(label_, style_.text_transform);
    auto run = te->Shape(resolved, display_label.c_str(),
                         static_cast<u32>(display_label.size()),
                         style_.font_size, style_.letter_spacing,
                         style_.line_height_multiplier);
    out_width = run.total_advance + style_.padding.horizontal();
    out_height = run.line_height + style_.padding.vertical();
}

void Button::OnPaint(Renderer2D& renderer) {
    // Use base Widget paint for shadow, background, border, gradient
    Widget::OnPaint(renderer);

    // Label
    auto* te = text_engine();
    FontHandle fh = effective_font();
    if (te && fh != kInvalidFont && !label_.empty()) {
        auto s = ComputedStyle();
        f32 alpha = s.opacity;

        // Resolve font for weight/style
        FontHandle resolved = fh;
        if (s.font_weight != FontWeight::kRegular || s.font_style != FontStyle::kNormal)
            resolved = te->ResolveFont(fh, s.font_weight, s.font_style);

        std::string display_label = apply_transform(label_, s.text_transform);
        auto run = te->Shape(resolved, display_label.c_str(),
                             static_cast<u32>(display_label.size()),
                             s.font_size, s.letter_spacing,
                             s.line_height_multiplier);

        // Center text in button
        f32 x = content_rect_.x + (content_rect_.w - run.total_advance) * 0.5f;
        f32 y = content_rect_.y + (content_rect_.h - run.line_height) * 0.5f;

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
}

} // namespace ugui
