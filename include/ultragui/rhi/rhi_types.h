#ifndef ULTRAGUI_RHI_RHI_TYPES_H_
#define ULTRAGUI_RHI_RHI_TYPES_H_

#include <ultragui/core/types.h>

namespace ugui {

using RHITextureHandle = u32;
constexpr RHITextureHandle kInvalidTexture = ~0u;

enum class RHIFormat : u8 {
    kR8Unorm,
    kRgba8Unorm,
    kBgra8Unorm,
    kRgba32Float,
};

enum class RHIFilter : u8 {
    kLinear,  // bilinear interpolation (smooth scaling, default for images)
    kNearest, // nearest-texel snap (pixel-perfect, use for text atlases)
};

} // namespace ugui

#endif  // ULTRAGUI_RHI_RHI_TYPES_H_
