#ifndef ULTRAGUI_RENDER_RENDERER2D_H_
#define ULTRAGUI_RENDER_RENDERER2D_H_

#include <ugui/core/color.h>
#include <ugui/core/rect.h>
#include <ugui/render/draw_data.h>
#include <ugui/render/texture_backend.h>
#include <ugui/render/vertex.h>
#include <ugui/rhi/rhi.h>
#include <ugui/style/enums.h>
#include <ugui/text/text_engine.h>

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
  void DrawTexturedRect(Rect rect, TextureId texture,
                        Color tint = Color::White(), u32 corner_radii = 0);

  // --- Visual effects ---

  /// Draw a rectangle with a linear gradient at the given angle.
  /// angle_deg follows CSS convention: 180 = top-to-bottom, 90 = left-to-right.
  void DrawRectGradient(Rect rect, Color start_color, Color end_color,
                        u32 corner_radii = 0, f32 angle_deg = 180.0f);

  /// Draw a box shadow behind a rectangle.
  /// blur = softness of edge, spread = expand beyond rect, offset = shadow
  /// displacement.
  void DrawShadow(Rect rect, Color shadow_color, f32 blur, f32 spread,
                  Vec2 offset, u32 corner_radii = 0);

  /// Draw an inset shadow inside a rectangle.
  void DrawInsetShadow(Rect rect, Color shadow_color, f32 blur, f32 spread,
                       Vec2 offset, u32 corner_radii = 0);

  /// Draw a rect with a border (fill + outline in one draw).
  void DrawBorderedRect(Rect rect, Color fill, Color border_color,
                        f32 border_width, u32 corner_radii = 0);

  // --- Text ---

  /// Draw shaped text at the given position.
  /// `pos` is the top-left origin; `run` comes from TextEngine::Shape().
  void DrawText(Vec2 pos, const TextRun& run, Color color,
                TextureId atlas_texture);

  /// Draw laid-out text (multi-line with alignment).
  void DrawTextLayout(Vec2 pos, const TextRun& run, const TextLayout& layout,
                      Color color, TextureId atlas_texture,
                      f32 max_width = 0.0f);

  /// Draw a radial gradient (center color fading to edge color).
  void DrawRadialGradient(Rect rect, Color center_color, Color edge_color,
                          u32 corner_radii = 0);

  /// Draw a multi-stop gradient (linear or radial).
  void DrawMultiStopGradient(Rect rect, const GradientStop* stops,
                             u32 stop_count, GradientType type, f32 angle_deg,
                             u32 corner_radii = 0);

  void PushScissor(Rect rect);
  void PopScissor();

  /// Finalize the current frame's batches and return them as a renderer-API
  /// agnostic draw list (the ImDrawData analog), instead of submitting via the
  /// RHI. Call after painting and instead of EndFrame(); a backend
  /// (see ugui_impl_*) renders the returned data. Pointers stay valid until
  /// the next BeginFrame(). Does not touch the RHI, so it works without a
  /// device.
  const DrawData& GetDrawData();

  /// Display (window-coordinate) viewport size, used to seed the default
  /// scissor when no RHI is attached (draw-data / embedded use).
  void set_display_size(Vec2 size) { display_size_ = size; }

  /// Texture sink for cached gradients (and any renderer-owned textures). In
  /// legacy mode this is the RHI adapter; in draw-data mode the host backend.
  /// May be null, in which case gradients fall back to flat color.
  void set_texture_backend(TextureBackend* backend) {
    texture_backend_ = backend;
  }

 private:
  void FlushBatch();
  void FlushTextBatch();
  void EmitQuad(Rect rect, u32 color, u32 color2, u32 corner_radii,
                f32 softness, f32 border_width, u32 border_color,
                TextureId texture);

  RHI* rhi_ = nullptr;
  TextureBackend* texture_backend_ = nullptr;

  struct DrawBatch {
    TextureId texture;
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
  TextureId current_text_atlas_ = kNullTextureId;

  // Submission order across both batch lists.
  Vector<DrawCommand> draw_order_;

  TextureId GetRadialGradientTexture(Color center, Color edge);
  TextureId GetMultiStopGradientTexture(const GradientStop* stops, u32 count,
                                        GradientType type, f32 angle_deg);

  Vector<Rect> scissor_stack_;
  HashMap<u64, TextureId> gradient_cache_;
  TextureId current_texture_ = kNullTextureId;
  Rect current_scissor_ = {};

  // Draw-data output (GetDrawData)
  Vector<DrawCmd> draw_cmds_;
  DrawData draw_data_;
  Vec2 display_size_ = {0.0f, 0.0f};
};

}  // namespace ugui

#endif  // ULTRAGUI_RENDER_RENDERER2D_H_
