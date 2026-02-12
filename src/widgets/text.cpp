#include <ultragui/render/renderer2d.h>
#include <ultragui/widgets/text.h>

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

void Text::measure(f32& out_width, f32& out_height) {
    auto* te = text_engine();
    FontHandle fh = effective_font();
    if (!te || fh == INVALID_FONT || text_.empty()) {
        out_width = 0;
        out_height = 0;
        return;
    }

    std::string display_text = apply_transform(text_, style_.text_transform);
    auto run =
        te->shape(fh, display_text.c_str(), static_cast<u32>(display_text.size()),
                  style_.font_size, style_.letter_spacing, style_.line_height_multiplier);
    out_width = run.total_advance;
    out_height = run.line_height;
}

void Text::on_paint(Renderer2D& renderer) {
    Widget::on_paint(renderer); // Background, shadow, border

    auto* te = text_engine();
    FontHandle fh = effective_font();
    if (!te || fh == INVALID_FONT || text_.empty())
        return;

    auto s = computed_style();
    f32 alpha = s.opacity;

    // Apply text transform
    std::string display_text = apply_transform(text_, s.text_transform);

    // Always shape fresh - the scratch buffer pointer from measure may be stale
    auto run =
        te->shape(fh, display_text.c_str(), static_cast<u32>(display_text.size()),
                  s.font_size, s.letter_spacing, s.line_height_multiplier);

    // Position text within content rect
    f32 x = content_rect_.x;
    f32 y = content_rect_.y;

    // Horizontal alignment
    switch (s.text_align) {
    case TextAlign::Center:
        x += (content_rect_.w - run.total_advance) * 0.5f;
        break;
    case TextAlign::Right:
        x += content_rect_.w - run.total_advance;
        break;
    default:
        break;
    }

    // Vertical center
    y += (content_rect_.h - run.line_height) * 0.5f;

    Color text_color = s.text_color.with_alpha(s.text_color.a * alpha);

    // Text shadow (drawn first, behind the main text)
    if (s.text_shadow_color.a > 0.0f) {
        Vec2 shadow_pos = {x + s.text_shadow_offset.x, y + s.text_shadow_offset.y};
        Color shadow_color = s.text_shadow_color.with_alpha(s.text_shadow_color.a * alpha);
        renderer.draw_text(shadow_pos, run, shadow_color, te->atlas_texture());
    }

    renderer.draw_text(Vec2{x, y}, run, text_color, te->atlas_texture());
}

} // namespace ugui
