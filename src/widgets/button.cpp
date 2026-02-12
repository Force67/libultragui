#include <ultragui/render/renderer2d.h>
#include <ultragui/text/text_engine.h>
#include <ultragui/widgets/button.h>

#include <cctype>
#include <string>

namespace ugui {

static std::string apply_transform(const std::string& s, TextTransform t) {
    if (t == TextTransform::None) return s;
    std::string out = s;
    if (t == TextTransform::Uppercase) {
        for (auto& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    } else if (t == TextTransform::Lowercase) {
        for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    } else if (t == TextTransform::Capitalize) {
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

void Button::measure(f32& out_width, f32& out_height) {
    auto* te = text_engine();
    FontHandle fh = effective_font();
    if (!te || fh == INVALID_FONT || label_.empty()) {
        out_width = 0;
        out_height = style_.font_size + style_.padding.vertical();
        return;
    }

    std::string display_label = apply_transform(label_, style_.text_transform);
    auto run = te->shape(fh, display_label.c_str(),
                         static_cast<u32>(display_label.size()),
                         style_.font_size, style_.letter_spacing,
                         style_.line_height_multiplier);
    out_width = run.total_advance + style_.padding.horizontal();
    out_height = run.line_height + style_.padding.vertical();
}

void Button::on_paint(Renderer2D& renderer) {
    // Use base Widget paint for shadow, background, border, gradient
    Widget::on_paint(renderer);

    // Label
    auto* te = text_engine();
    FontHandle fh = effective_font();
    if (te && fh != INVALID_FONT && !label_.empty()) {
        auto s = computed_style();
        f32 alpha = s.opacity;

        std::string display_label = apply_transform(label_, s.text_transform);
        auto run = te->shape(fh, display_label.c_str(),
                             static_cast<u32>(display_label.size()),
                             s.font_size, s.letter_spacing,
                             s.line_height_multiplier);

        // Center text in button
        f32 x = content_rect_.x + (content_rect_.w - run.total_advance) * 0.5f;
        f32 y = content_rect_.y + (content_rect_.h - run.line_height) * 0.5f;

        Color text_color = s.text_color.with_alpha(s.text_color.a * alpha);

        // Text shadow (drawn first, behind the main text)
        if (s.text_shadow_color.a > 0.0f) {
            Vec2 shadow_pos = {x + s.text_shadow_offset.x, y + s.text_shadow_offset.y};
            Color shadow_color = s.text_shadow_color.with_alpha(s.text_shadow_color.a * alpha);
            renderer.draw_text(shadow_pos, run, shadow_color, te->atlas_texture());
        }

        renderer.draw_text(Vec2{x, y}, run, text_color, te->atlas_texture());
    }
}

} // namespace ugui
