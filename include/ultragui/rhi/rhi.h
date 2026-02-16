#ifndef ULTRAGUI_RHI_RHI_H_
#define ULTRAGUI_RHI_RHI_H_

#include <ultragui/core/color.h>
#include <ultragui/core/math.h>
#include <ultragui/core/rect.h>
#include <ultragui/render/vertex.h>
#include <ultragui/rhi/rhi_types.h>

namespace ugui {

class Platform;

struct RHIConfig {
    Platform* platform = nullptr;
    bool validation = true;
    bool vsync = true;
    const char* shader_dir = nullptr;
};

/// Minimal GPU abstraction for 2D UI rendering.
/// Not a general-purpose GPU API - purpose-built for batched quad drawing.
class RHI {
public:
    virtual ~RHI() = default;

    virtual bool Init(const RHIConfig& config) = 0;
    virtual void Shutdown() = 0;

    virtual bool BeginFrame(Color clear_color) = 0;
    virtual void EndFrame() = 0;

    virtual void SetScissor(Rect rect) = 0;
    virtual void ResetScissor() = 0;

    /// Upload and draw a batch of 2D vertices.
    /// If texture is kInvalidTexture, the white fallback texture is used.
    virtual void DrawTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                                u32 index_count, RHITextureHandle texture = kInvalidTexture) = 0;

    /// Draw text glyphs using the R8 alpha-only pipeline.
    virtual void DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                                     u32 index_count, RHITextureHandle atlas_texture) = 0;

    virtual RHITextureHandle CreateTexture(u32 width, u32 height, RHIFormat format,
                                            const void* pixels,
                                            RHIFilter filter = RHIFilter::kLinear) = 0;
    virtual void UpdateTexture(RHITextureHandle handle, const void* pixels) = 0;
    virtual void DestroyTexture(RHITextureHandle handle) = 0;

    /// Acquire the next frame (fence wait, swapchain image, command buffer begin)
    /// without starting a render pass. Call this before BeginOffscreen() if you
    /// need offscreen passes before the swapchain pass. If not called explicitly,
    /// BeginFrame() will call it internally.
    virtual bool AcquireFrame() = 0;

    /// Create an offscreen render target that can be drawn into and sampled as a texture.
    /// Returns a texture handle usable with both BeginOffscreen() and DrawTexturedRect().
    virtual RHITextureHandle CreateRenderTarget(u32 width, u32 height) = 0;

    /// Destroy a render target and release all associated GPU resources.
    virtual void DestroyRenderTarget(RHITextureHandle handle) = 0;

    /// Begin rendering to an offscreen target. Must be called after AcquireFrame()
    /// and outside any other render pass.
    virtual bool BeginOffscreen(RHITextureHandle target, Color clear_color) = 0;

    /// End the offscreen render pass and transition the image to shader-readable layout.
    virtual void EndOffscreen(RHITextureHandle target) = 0;

    virtual Vec2 display_size() const = 0;

    /// Ratio of framebuffer pixels to window coordinates.
    /// 1.0 on standard displays, 2.0 on typical HiDPI/Retina displays.
    virtual f32 dpi_scale() const = 0;
};

RHI* CreateVulkanRhi();

} // namespace ugui

#endif  // ULTRAGUI_RHI_RHI_H_
