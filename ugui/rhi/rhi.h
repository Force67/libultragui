#ifndef ULTRAGUI_RHI_RHI_H_
#define ULTRAGUI_RHI_RHI_H_

#include <ugui/core/color.h>
#include <ugui/core/math.h>
#include <ugui/core/rect.h>
#include <ugui/render/vertex.h>
#include <ugui/rhi/rhi_types.h>

namespace ugui {

class Platform;

struct RHIConfig {
  Platform* platform = nullptr;
  bool validation = true;
  bool vsync = true;
  const char* shader_dir = nullptr;
  /// Embedded mode: the host owns the frame. The RHI renders into the existing
  /// (host) surface without clearing it on BeginFrame or presenting on
  /// EndFrame; the host is responsible for clearing and presenting. Used when
  /// dropping ultragui on top of an application's own render pipeline.
  bool embedded = false;
};

/// Concrete GPU abstraction with link-time swappable implementation.
/// The default implementation uses Vulkan; swap the .cc file via CMake
/// (ULTRAGUI_RHI_SOURCE) to provide your own backend.
class RHI {
 public:
  RHI();
  ~RHI();
  RHI(const RHI&) = delete;
  RHI& operator=(const RHI&) = delete;

  bool Init(const RHIConfig& config);
  void Shutdown();

  bool BeginFrame(Color clear_color);
  void EndFrame();

  void SetScissor(Rect rect);
  void ResetScissor();

  /// Upload and draw a batch of 2D vertices.
  /// If texture is kInvalidTexture, the white fallback texture is used.
  void DrawTriangles(const Vertex2D* vertices, u32 vertex_count,
                     const u32* indices, u32 index_count,
                     RHITextureHandle texture = kInvalidTexture);

  /// Draw text glyphs using the R8 alpha-only pipeline.
  void DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count,
                         const u32* indices, u32 index_count,
                         RHITextureHandle atlas_texture);

  RHITextureHandle CreateTexture(u32 width, u32 height, RHIFormat format,
                                 const void* pixels,
                                 RHIFilter filter = RHIFilter::kLinear);
  void UpdateTexture(RHITextureHandle handle, const void* pixels);
  void DestroyTexture(RHITextureHandle handle);

  /// Acquire the next frame (fence wait, swapchain image, command buffer begin)
  /// without starting a render pass. Call this before BeginOffscreen() if you
  /// need offscreen passes before the swapchain pass. If not called explicitly,
  /// BeginFrame() will call it internally.
  bool AcquireFrame();

  /// Create an offscreen render target that can be drawn into and sampled as a
  /// texture. Returns a texture handle usable with both BeginOffscreen() and
  /// DrawTexturedRect().
  RHITextureHandle CreateRenderTarget(u32 width, u32 height);

  /// Destroy a render target and release all associated GPU resources.
  void DestroyRenderTarget(RHITextureHandle handle);

  /// Begin rendering to an offscreen target. Must be called after
  /// AcquireFrame() and outside any other render pass.
  bool BeginOffscreen(RHITextureHandle target, Color clear_color);

  /// End the offscreen render pass and transition the image to shader-readable
  /// layout.
  void EndOffscreen(RHITextureHandle target);

  /// Convert YCbCr planes to RGBA by rendering a fullscreen pass with the video
  /// shader into an offscreen render target. The target must be a render target
  /// created with CreateRenderTarget(). y/cb/cr are R8 textures for the three
  /// planes. Must be called after AcquireFrame(), outside any other render
  /// pass.
  void ConvertVideoFrame(RHITextureHandle target, RHITextureHandle y,
                         RHITextureHandle cb, RHITextureHandle cr);

  Vec2 display_size() const;

  /// Ratio of framebuffer pixels to window coordinates.
  /// 1.0 on standard displays, 2.0 on typical HiDPI/Retina displays.
  f32 dpi_scale() const;

  struct Impl;

 private:
  Impl* impl_;
};

}  // namespace ugui

#endif  // ULTRAGUI_RHI_RHI_H_
