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

  /// Push a clockwise rotation (degrees) about `pivot`. Composes with the
  /// outer transform, so a rotated parent rotates its subtree. Baked into
  /// vertex positions; shaders unchanged. Scissor rects stay axis-aligned.
  void PushTransform(Vec2 pivot, f32 degrees);
  void PopTransform();

  /// Finalize batches and return a renderer-agnostic draw list instead of
  /// submitting via the RHI. Call after painting, instead of EndFrame().
  /// Valid until the next BeginFrame(). No RHI required.
  const DrawData& GetDrawData();

  /// Display (window-coordinate) viewport size, used to seed the default
  /// scissor when no RHI is attached (draw-data / embedded use).
  void set_display_size(Vec2 size) { display_size_ = size; }

  /// Texture sink for cached gradients. Legacy mode: RHI adapter; draw-data
  /// mode: host backend. Null means gradients fall back to flat color.
  void set_texture_backend(TextureBackend* backend) {
    texture_backend_ = backend;
  }

  /// Backdrop-blur radius (px) for the next plain quads. Reset to 0 after
  /// the blurred quad.
  void set_next_blur(f32 radius) { next_blur_ = radius; }

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
    f32 blur = 0.0f;  // backdrop-blur radius for this batch (0 = none)
  };

  // Commands point into batches_ or text_batches_. EndFrame walks
  // draw_order_ so quads and text reach the GPU in emission order;
  // otherwise earlier text renders on top of later backgrounds,
  // breaking modal overlays.
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

  // 2D affine transform applied to vertex positions:
  //   (x', y') = (a*x + b*y + tx, c*x + d*y + ty)
  struct Affine2D {
    f32 a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
    bool identity = true;
    Vec2 Apply(Vec2 p) const {
      return {a * p.x + b * p.y + tx, c * p.x + d * p.y + ty};
    }
  };
  Affine2D xform_;             // current (composed) transform
  Vector<Affine2D> xform_stack_;

  Vector<Rect> scissor_stack_;
  HashMap<u64, TextureId> gradient_cache_;
  TextureId current_texture_ = kNullTextureId;
  Rect current_scissor_ = {};
  f32 next_blur_ = 0.0f;   // caller-set blur for upcoming quads
  f32 batch_blur_ = 0.0f;  // blur of the currently accumulating quad batch

  // Draw-data output (GetDrawData)
  Vector<DrawCmd> draw_cmds_;
  DrawData draw_data_;
  Vec2 display_size_ = {0.0f, 0.0f};
};

}  // namespace ugui

#endif  // ULTRAGUI_RENDER_RENDERER2D_H_
