#include <ultragui/render/renderer2d.h>
#include <ultragui/widgets/rich_text.h>

#include <algorithm>
#include <cmath>

namespace ugui {

f32 RichText::LayoutSpans(Vector<ShapedSpan>& out, f32 max_width) const {
    auto* te = text_engine();
    FontHandle base_fh = effective_font();
    if (!te || base_fh == kInvalidFont) return 0.0f;

    const auto& s = style_;
    f32 line_x = 0.0f;
    f32 line_y = 0.0f;
    f32 line_height = 0.0f;

    for (const auto& span : spans_) {
        f32 size = span.font_size > 0.0f ? span.font_size : s.font_size;

        FontHandle fh = base_fh;
        if (span.font_weight != FontWeight::kRegular ||
            span.font_style != FontStyle::kNormal) {
            fh = te->ResolveFont(base_fh, span.font_weight, span.font_style);
        }

        auto run = te->Shape(fh, span.text.c_str(),
                             static_cast<u32>(span.text.size()), size,
                             s.letter_spacing, s.line_height_multiplier);

        // Wrap to next line if this span exceeds available width
        // (only when we are not at the start of a line)
        if (line_x > 0.0f && max_width > 0.0f &&
            line_x + run.total_advance > max_width) {
            line_y += line_height;
            line_x = 0.0f;
            line_height = 0.0f;
        }

        Color c = span.color;

        out.push_back(ShapedSpan{run, c, span.decoration, size, line_x, line_y});

        line_x += run.total_advance;
        line_height = std::max(line_height, run.line_height);
    }

    return line_y + line_height;
}

void RichText::Measure(f32& out_width, f32& out_height) {
    auto* te = text_engine();
    if (!te || effective_font() == kInvalidFont || spans_.empty()) {
        out_width = 0;
        out_height = 0;
        return;
    }

    Vector<ShapedSpan> shaped;
    f32 total_h = LayoutSpans(shaped, 1e6f);  // Measure without wrapping

    f32 max_w = 0;
    for (const auto& ss : shaped) {
        max_w = std::max(max_w, ss.x + ss.run.total_advance);
    }
    out_width = max_w;
    out_height = total_h;
}

void RichText::OnPaint(Renderer2D& renderer) {
    Widget::OnPaint(renderer);  // Background, shadow, border

    auto* te = text_engine();
    if (!te || spans_.empty()) return;

    auto s = ComputedStyle();
    f32 alpha = s.opacity;

    Vector<ShapedSpan> shaped;
    LayoutSpans(shaped, content_rect_.w);

    for (const auto& ss : shaped) {
        f32 x = content_rect_.x + ss.x;
        f32 y = content_rect_.y + ss.y;
        Color c = ss.color.WithAlpha(ss.color.a * alpha);

        renderer.DrawText(Vec2{x, y}, ss.run, c, te->atlas_texture());

        // Text decoration (underline / strikethrough)
        if (ss.decoration != TextDecoration::kNone) {
            f32 thickness = std::max(1.0f, ss.font_size / 14.0f);
            f32 baseline = y + ss.run.ascent;

            if (HasDecoration(ss.decoration, TextDecoration::kUnderline)) {
                f32 line_y = baseline + ss.font_size * 0.15f;
                renderer.DrawRect({x, line_y, ss.run.total_advance, thickness},
                                  c);
            }
            if (HasDecoration(ss.decoration, TextDecoration::kStrikethrough)) {
                f32 line_y = baseline - ss.font_size * 0.3f;
                renderer.DrawRect({x, line_y, ss.run.total_advance, thickness},
                                  c);
            }
        }
    }
}

}  // namespace ugui
