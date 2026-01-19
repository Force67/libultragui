#pragma once

#include <ultragui/core/types.h>

namespace ugui {

using RHITextureHandle = u32;
constexpr RHITextureHandle INVALID_TEXTURE = ~0u;

enum class RHIFormat : u8 {
    R8_UNORM,
    RGBA8_UNORM,
    BGRA8_UNORM,
    RGBA32_FLOAT,
};

} // namespace ugui
