#pragma once

#include <ultragui/rhi/rhi_types.h>
#include <ultragui/widgets/widget.h>

namespace ugui {

/// Image display widget.
class Image : public Widget {
public:
    using Widget::Widget;

    void set_texture(RHITextureHandle texture, f32 width, f32 height) {
        texture_ = texture;
        natural_w_ = width;
        natural_h_ = height;
        mark_dirty();
    }

    void measure(f32& out_width, f32& out_height) override;
    void on_paint(Renderer2D& renderer) override;

private:
    RHITextureHandle texture_ = INVALID_TEXTURE;
    f32 natural_w_ = 0, natural_h_ = 0;
};

} // namespace ugui
