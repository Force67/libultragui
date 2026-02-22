#ifndef ULTRAGUI_SVG_SVG_H_
#define ULTRAGUI_SVG_SVG_H_

#include <ultragui/core/types.h>
#include <ultragui/rhi/rhi_types.h>

namespace ugui {

class RHI;

/// CPU-side rasterized SVG image (RGBA8 pixels).
struct SvgImage {
    u32 width = 0;
    u32 height = 0;
    Vector<u8> pixels; // RGBA8, size = width * height * 4
};

/// Load and rasterize an SVG file to RGBA pixels.
/// If target_width or target_height is 0, derives size from the SVG's viewBox/dimensions.
bool LoadSvg(const char* path, SvgImage& out, u32 target_width = 0, u32 target_height = 0);

/// Load and rasterize SVG from a memory buffer.
bool LoadSvgMemory(const char* data, usize length, SvgImage& out,
                     u32 target_width = 0, u32 target_height = 0);

/// Load SVG, rasterize, and create an RHI texture. Returns kInvalidTexture on failure.
RHITextureHandle LoadSvgTexture(RHI* rhi, const char* path,
                                  u32 target_width = 0, u32 target_height = 0);

} // namespace ugui

#endif  // ULTRAGUI_SVG_SVG_H_
