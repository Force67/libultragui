#ifndef ULTRAGUI_RENDER_RENDERER2D_H_
#define ULTRAGUI_RENDER_RENDERER2D_H_

#include <ultragui/core/color.h>
#include <ultragui/core/rect.h>
#include <ultragui/render/vertex.h>
#include <ultragui/rhi/rhi.h>
#include <ultragui/style/enums.h>
#include <ultragui/text/text_engine.h>

namespace ugui {

struct GradientStop;

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

    /// Draw a rectangle with a linear gradient at the given angle.
    /// angle_deg follows CSS convention: 180 = top-to-bottom, 90 = left-to-right.
    void DrawRectGradient(Rect rect, Color start_color, Color end_color,
                            u32 corner_radii = 0, f32 angle_deg = 180.0f);

    /// Draw a box shadow behind a rectangle.
    /// blur = softness of edge, spread = expand beyond rect, offset = shadow displacement.
    void DrawShadow(Rect rect, Color shadow_color, f32 blur, f32 spread, Vec2 offset,
                     u32 corner_radii = 0);

    /// Draw an inset shadow inside a rectangle.
    void DrawInsetShadow(Rect rect, Color shadow_color, f32 blur, f32 spread, Vec2 offset,
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

    /// Draw a radial gradient (center color fading to edge color).
    void DrawRadialGradient(Rect rect, Color center_color, Color edge_color,
                             u32 corner_radii = 0);

    /// Draw a multi-stop gradient (linear or radial).
    void DrawMultiStopGradient(Rect rect, const GradientStop* stops, u32 stop_count,
                                GradientType type, f32 angle_deg, u32 corner_radii = 0);

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

    // Each draw command points at a slot in either `batches_` or
    // `text_batches_`. EndFrame walks `draw_order_` rather than the two
    // batch vectors directly so quads and text are submitted to the GPU
    // in the order they were emitted by the widget tree - without this,
    // text from any earlier widget renders on top of any later widget's
    // background, breaking modal overlays.
    enum class DrawKind : u8 {
        kQuad,
        kText,
    };
    struct DrawCommand {
        DrawKind kind;
        u32 batch_index;
    };

    // Quad batching
    Vector<Vertex2D> vertices_;
    Vector<u32> indices_;
    Vector<DrawBatch> batches_;

    // Text batching (separate pass with text pipeline)
    Vector<Vertex2D> text_vertices_;
    Vector<u32> text_indices_;
    Vector<DrawBatch> text_batches_;
    RHITextureHandle current_text_atlas_ = kInvalidTexture;

    // Submission order across both batch lists.
    Vector<DrawCommand> draw_order_;

    RHITextureHandle GetRadialGradientTexture(Color center, Color edge);
    RHITextureHandle GetMultiStopGradientTexture(const GradientStop* stops, u32 count,
                                                  GradientType type, f32 angle_deg);

    Vector<Rect> scissor_stack_;
    HashMap<u64, RHITextureHandle> gradient_cache_;
    RHITextureHandle current_texture_ = kInvalidTexture;
    Rect current_scissor_ = {};
};

} // namespace ugui

#endif  // ULTRAGUI_RENDER_RENDERER2D_H_
