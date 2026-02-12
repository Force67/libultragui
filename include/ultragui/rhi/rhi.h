#pragma once

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

    virtual bool init(const RHIConfig& config) = 0;
    virtual void shutdown() = 0;

    virtual bool begin_frame(Color clear_color) = 0;
    virtual void end_frame() = 0;

    virtual void set_scissor(Rect rect) = 0;
    virtual void reset_scissor() = 0;

    /// Upload and draw a batch of 2D vertices.
    /// If texture is INVALID_TEXTURE, the white fallback texture is used.
    virtual void draw_triangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                                u32 index_count, RHITextureHandle texture = INVALID_TEXTURE) = 0;

    /// Draw text glyphs using the R8 alpha-only pipeline.
    virtual void draw_text_triangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                                     u32 index_count, RHITextureHandle atlas_texture) = 0;

    virtual RHITextureHandle create_texture(u32 width, u32 height, RHIFormat format,
                                            const void* pixels,
                                            RHIFilter filter = RHIFilter::Linear) = 0;
    virtual void update_texture(RHITextureHandle handle, const void* pixels) = 0;
    virtual void destroy_texture(RHITextureHandle handle) = 0;

    /// Acquire the next frame (fence wait, swapchain image, command buffer begin)
    /// without starting a render pass. Call this before begin_offscreen() if you
    /// need offscreen passes before the swapchain pass. If not called explicitly,
    /// begin_frame() will call it internally.
    virtual bool acquire_frame() = 0;

    /// Create an offscreen render target that can be drawn into and sampled as a texture.
    /// Returns a texture handle usable with both begin_offscreen() and draw_textured_rect().
    virtual RHITextureHandle create_render_target(u32 width, u32 height) = 0;

    /// Destroy a render target and release all associated GPU resources.
    virtual void destroy_render_target(RHITextureHandle handle) = 0;

    /// Begin rendering to an offscreen target. Must be called after acquire_frame()
    /// and outside any other render pass.
    virtual bool begin_offscreen(RHITextureHandle target, Color clear_color) = 0;

    /// End the offscreen render pass and transition the image to shader-readable layout.
    virtual void end_offscreen(RHITextureHandle target) = 0;

    virtual Vec2 display_size() const = 0;

    /// Ratio of framebuffer pixels to window coordinates.
    /// 1.0 on standard displays, 2.0 on typical HiDPI/Retina displays.
    virtual f32 dpi_scale() const = 0;
};

RHI* create_vulkan_rhi();

} // namespace ugui
