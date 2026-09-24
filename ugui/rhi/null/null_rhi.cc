// An RHI with no device, for hosts that only run ultragui in draw-data mode
// (UIConfig::draw_data) and render through a backend of their own, such as
// ugui_impl_dx11. UIContext never initialises the RHI in that mode, but it
// still links one; this one has nothing to link against.
#include <ugui/rhi/rhi.h>

namespace ugui {

struct RHI::Impl {};

RHI::RHI() : impl_(nullptr) {}
RHI::~RHI() { delete impl_; }

bool RHI::Init(const RHIConfig&) { return false; }
void RHI::Shutdown() {}
bool RHI::BeginFrame(Color) { return false; }
void RHI::EndFrame() {}
void RHI::SetScissor(Rect) {}
void RHI::ResetScissor() {}
void RHI::DrawTriangles(const Vertex2D*, u32, const u32*, u32,
                        RHITextureHandle) {}
void RHI::DrawTextTriangles(const Vertex2D*, u32, const u32*, u32,
                            RHITextureHandle) {}
RHITextureHandle RHI::CreateTexture(u32, u32, RHIFormat, const void*,
                                    RHIFilter) {
  return kInvalidTexture;
}
void RHI::UpdateTexture(RHITextureHandle, const void*) {}
void RHI::DestroyTexture(RHITextureHandle) {}
bool RHI::AcquireFrame() { return false; }
RHITextureHandle RHI::CreateRenderTarget(u32, u32) { return kInvalidTexture; }
void RHI::DestroyRenderTarget(RHITextureHandle) {}
bool RHI::BeginOffscreen(RHITextureHandle, Color) { return false; }
void RHI::EndOffscreen(RHITextureHandle) {}
void RHI::ConvertVideoFrame(RHITextureHandle, RHITextureHandle,
                            RHITextureHandle, RHITextureHandle) {}
Vec2 RHI::display_size() const { return {0.0f, 0.0f}; }
f32 RHI::dpi_scale() const { return 1.0f; }

}  // namespace ugui
