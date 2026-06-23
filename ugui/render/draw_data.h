#ifndef ULTRAGUI_RENDER_DRAW_DATA_H_
#define ULTRAGUI_RENDER_DRAW_DATA_H_

#include <ugui/core/math.h>
#include <ugui/core/rect.h>
#include <ugui/core/types.h>
#include <ugui/render/vertex.h>

namespace ugui {

/// Opaque, host-assigned texture identifier carried by draw commands, like
/// Dear ImGui's ImTextureID. A renderer backend fills/interprets these as
/// values meaningful to its graphics API (e.g. a GL texture name, or a
/// VkDescriptorSet cast to a u64).
using TextureId = u64;

inline constexpr TextureId kNullTextureId = 0;      ///< no texture (white)
inline constexpr TextureId kFontTextureId = ~0ull;  ///< the glyph atlas

/// One draw command: a contiguous run of indices sharing a clip rect, a
/// texture, and a pipeline. Mirrors ImDrawCmd.
struct DrawCmd {
  Rect clip_rect;  ///< scissor, in display (window) coordinates
  TextureId texture_id = kNullTextureId;
  u32 index_offset = 0;  ///< first index into the relevant index buffer
  u32 elem_count = 0;    ///< number of indices to draw
  bool is_text = false;  ///< text pipeline + glyph buffers vs quad pipeline
  /// Backdrop blur radius (px) for this batch's quads (frosted glass). 0 = none.
  /// A backend with a captured, blurred copy of what is behind the UI fills the
  /// quad with it instead of sampling the command's texture.
  f32 blur = 0.0f;
};

/// All geometry and commands produced for one UI frame, like ImDrawData.
///
/// ultragui uses two pipelines (an SDF rounded-rect "quad" pipeline and an
/// alpha-only "text" pipeline) with separate vertex/index buffers, so a
/// backend uploads both buffers and selects per command via DrawCmd::is_text.
/// All pointers are owned by the renderer and remain valid until the next
/// frame is built.
struct DrawData {
  const Vertex2D* quad_vertices = nullptr;
  u32 quad_vertex_count = 0;
  const u32* quad_indices = nullptr;
  u32 quad_index_count = 0;

  const Vertex2D* text_vertices = nullptr;
  u32 text_vertex_count = 0;
  const u32* text_indices = nullptr;
  u32 text_index_count = 0;

  const DrawCmd* commands = nullptr;
  u32 command_count = 0;

  Vec2 display_pos = {0.0f, 0.0f};        ///< top-left in display coords
  Vec2 display_size = {0.0f, 0.0f};       ///< size in display coords
  Vec2 framebuffer_scale = {1.0f, 1.0f};  ///< display -> framebuffer px (dpi)

  bool valid = false;
};

}  // namespace ugui

#endif  // ULTRAGUI_RENDER_DRAW_DATA_H_
