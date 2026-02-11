#pragma once

#include <ultragui/core/types.h>
#include <ultragui/rhi/rhi_types.h>

#include <vector>

namespace ugui {

class RHI;

/// CPU-side rasterized SVG image (RGBA8 pixels).
struct SvgImage {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> pixels; // RGBA8, size = width * height * 4
};

/// Load and rasterize an SVG file to RGBA pixels.
/// If target_width or target_height is 0, derives size from the SVG's viewBox/dimensions.
bool load_svg(const char* path, SvgImage& out, u32 target_width = 0, u32 target_height = 0);

/// Load and rasterize SVG from a memory buffer.
bool load_svg_memory(const char* data, usize length, SvgImage& out,
                     u32 target_width = 0, u32 target_height = 0);

/// Load SVG, rasterize, and create an RHI texture. Returns INVALID_TEXTURE on failure.
RHITextureHandle load_svg_texture(RHI* rhi, const char* path,
                                  u32 target_width = 0, u32 target_height = 0);

} // namespace ugui
