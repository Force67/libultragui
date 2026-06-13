#ifndef ULTRAGUI_RHI_RHI_TEXTURE_BACKEND_H_
#define ULTRAGUI_RHI_RHI_TEXTURE_BACKEND_H_

#include <ugui/render/texture_backend.h>
#include <ugui/rhi/rhi.h>

namespace ugui {

/// TextureBackend that forwards to the bundled device-owning RHI. Used in
/// legacy mode so subsystems can create textures through the same seam the
/// draw-data path uses. Maps the RHI's u32 handles into the TextureId space via
/// the +1 bias (see TextureIdFromRhiHandle).
class RHITextureBackend : public TextureBackend {
 public:
  explicit RHITextureBackend(RHI* rhi) : rhi_(rhi) {}

  TextureId CreateTexture(u32 width, u32 height, RHIFormat format,
                          const void* pixels, RHIFilter filter) override {
    if (!rhi_) return kNullTextureId;
    return TextureIdFromRhiHandle(
        rhi_->CreateTexture(width, height, format, pixels, filter));
  }

  void UpdateTexture(TextureId id, const void* pixels) override {
    if (rhi_) rhi_->UpdateTexture(RhiHandleFromTextureId(id), pixels);
  }

  void DestroyTexture(TextureId id) override {
    if (rhi_) rhi_->DestroyTexture(RhiHandleFromTextureId(id));
  }

 private:
  RHI* rhi_;
};

}  // namespace ugui

#endif  // ULTRAGUI_RHI_RHI_TEXTURE_BACKEND_H_
