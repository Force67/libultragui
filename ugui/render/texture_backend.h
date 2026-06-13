#ifndef ULTRAGUI_RENDER_TEXTURE_BACKEND_H_
#define ULTRAGUI_RENDER_TEXTURE_BACKEND_H_

#include <ugui/render/draw_data.h>
#include <ugui/rhi/rhi_types.h>

namespace ugui {

/// Host-providable GPU texture sink, the Dear ImGui style middleware seam.
/// ultragui never owns a graphics device; whoever does (the bundled RHI in
/// legacy mode, or the application's own renderer in draw-data mode) implements
/// this so SVG, Image, Lottie, and vector-animation textures flow through one
/// opaque TextureId space regardless of which path is active.
///
/// TextureId conventions (see draw_data.h): 0 = none/white, ~0 = font atlas,
/// anything else = a real texture meaningful to the backend that minted it.
struct TextureBackend {
  virtual ~TextureBackend() = default;

  /// Upload `pixels` (format-dependent, tightly packed) and return an id.
  /// Returns kNullTextureId on failure.
  virtual TextureId CreateTexture(u32 width, u32 height, RHIFormat format,
                                  const void* pixels,
                                  RHIFilter filter = RHIFilter::kLinear) = 0;

  /// Re-upload pixels into an existing texture (same dimensions/format).
  virtual void UpdateTexture(TextureId id, const void* pixels) = 0;

  /// Release a texture. No-op for kNullTextureId / kFontTextureId.
  virtual void DestroyTexture(TextureId id) = 0;
};

/// Map the RHI's internal u32 handle space into the unified TextureId space,
/// keeping 0 ("none/white") and ~0 ("font atlas") reserved. A valid RHI handle
/// h becomes h + 1, so even RHI handle 0 lands on a non-null id.
inline TextureId TextureIdFromRhiHandle(RHITextureHandle h) {
  return h == kInvalidTexture ? kNullTextureId : static_cast<TextureId>(h) + 1;
}

/// Inverse of TextureIdFromRhiHandle. The reserved ids (none, font) map back to
/// kInvalidTexture so the legacy RHI submit path falls through to its white /
/// atlas fallbacks.
inline RHITextureHandle RhiHandleFromTextureId(TextureId id) {
  return (id == kNullTextureId || id == kFontTextureId)
             ? kInvalidTexture
             : static_cast<RHITextureHandle>(id - 1);
}

}  // namespace ugui

#endif  // ULTRAGUI_RENDER_TEXTURE_BACKEND_H_
