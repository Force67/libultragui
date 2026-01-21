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
                                            const void* pixels) = 0;
    virtual void destroy_texture(RHITextureHandle handle) = 0;

    virtual Vec2 display_size() const = 0;
};

RHI* create_vulkan_rhi();

} // namespace ugui
