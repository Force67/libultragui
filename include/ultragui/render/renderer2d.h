#ifndef ULTRAGUI_RENDER_RENDERER2D_H_
#define ULTRAGUI_RENDER_RENDERER2D_H_

#include <ultragui/core/color.h>
#include <ultragui/core/rect.h>
#include <ultragui/render/vertex.h>
#include <ultragui/rhi/rhi.h>
#include <ultragui/text/text_engine.h>

#include <vector>

namespace ugui {

/// High-level batched 2D renderer. Collects draw commands during a frame,
/// then flushes them as GPU draw calls via the RHI.
class Renderer2D {
public:
    bool Init(RHI* rhi);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    // --- Solid color drawing ---
    void DrawRect(Rect rect, Color color, u32 corner_radii = 0);
    void DrawTexturedRect(Rect rect, RHITextureHandle texture, Color tint = Color::White(),
                            u32 corner_radii = 0);

    // --- Visual effects ---

    /// Draw a rectangle with a linear gradient (top-to-bottom).
    void DrawRectGradient(Rect rect, Color top_color, Color bottom_color,
                            u32 corner_radii = 0);

    /// Draw a box shadow behind a rectangle.
    /// blur = softness of edge, spread = expand beyond rect, offset = shadow displacement.
    void DrawShadow(Rect rect, Color shadow_color, f32 blur, f32 spread, Vec2 offset,
                     u32 corner_radii = 0);

    /// Draw a rect with a border (fill + outline in one draw).
    void DrawBorderedRect(Rect rect, Color fill, Color border_color, f32 border_width,
                            u32 corner_radii = 0);

    // --- Text ---

    /// Draw shaped text at the given position.
    /// `pos` is the top-left origin; `run` comes from TextEngine::Shape().
    void DrawText(Vec2 pos, const TextRun& run, Color color, RHITextureHandle atlas_texture);

    /// Draw laid-out text (multi-line with alignment).
    void DrawTextLayout(Vec2 pos, const TextRun& run, const TextLayout& layout, Color color,
                          RHITextureHandle atlas_texture, f32 max_width = 0.0f);

    void PushScissor(Rect rect);
    void PopScissor();

private:
    void FlushBatch();
    void FlushTextBatch();
    void EmitQuad(Rect rect, u32 color, u32 color2, u32 corner_radii, f32 softness,
                   f32 border_width, u32 border_color, RHITextureHandle texture);

    RHI* rhi_ = nullptr;

    struct DrawBatch {
        RHITextureHandle texture;
        Rect scissor;
        u32 index_offset;
        u32 index_count;
    };

    // Quad batching
    std::vector<Vertex2D> vertices_;
    std::vector<u32> indices_;
    std::vector<DrawBatch> batches_;

    // Text batching (separate pass with text pipeline)
    std::vector<Vertex2D> text_vertices_;
    std::vector<u32> text_indices_;
    std::vector<DrawBatch> text_batches_;
    RHITextureHandle current_text_atlas_ = kInvalidTexture;

    std::vector<Rect> scissor_stack_;
    RHITextureHandle current_texture_ = kInvalidTexture;
    Rect current_scissor_ = {};
};

} // namespace ugui

#endif  // ULTRAGUI_RENDER_RENDERER2D_H_
