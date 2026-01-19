#pragma once

#include <ultragui/core/color.h>
#include <ultragui/core/rect.h>
#include <ultragui/render/vertex.h>
#include <ultragui/rhi/rhi.h>

#include <vector>

namespace ugui {

/// High-level batched 2D renderer. Collects draw commands during a frame,
/// then flushes them as GPU draw calls via the RHI.
class Renderer2D {
public:
    bool init(RHI* rhi);
    void shutdown();

    void begin_frame();
    void end_frame();

    void draw_rect(Rect rect, Color color, f32 corner_radius = 0.0f);
    void draw_textured_rect(Rect rect, RHITextureHandle texture, Color tint = Color::white(),
                            f32 corner_radius = 0.0f);

    void push_scissor(Rect rect);
    void pop_scissor();

private:
    void flush_batch();
    void emit_quad(Rect rect, u32 color, f32 corner_radius, RHITextureHandle texture);

    RHI* rhi_ = nullptr;

    struct DrawBatch {
        RHITextureHandle texture;
        Rect scissor;
        u32 index_offset;
        u32 index_count;
    };

    std::vector<Vertex2D> vertices_;
    std::vector<u32> indices_;
    std::vector<DrawBatch> batches_;
    std::vector<Rect> scissor_stack_;

    RHITextureHandle current_texture_ = INVALID_TEXTURE;
    Rect current_scissor_ = {};
};

} // namespace ugui
