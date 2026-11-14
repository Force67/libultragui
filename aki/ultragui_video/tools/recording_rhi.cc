// A C++ RHI (the class ugui/rhi/rhi.h declares, swapped at link time) that
// records what the VideoPlayer does to it: texture contents, handles, and the
// ConvertVideoFrame calls. Link it instead of ugui/rhi/null/null_rhi.cc;
// gen_reference.cc reads the state below.
#include <ugui/rhi/rhi.h>

#include <string.h>

#include <map>
#include <vector>

namespace ugui {

struct RecordingState {
  std::map<RHITextureHandle, std::vector<u8>> textures;
  std::map<RHITextureHandle, u32> widths, heights;
  std::map<RHITextureHandle, bool> is_target;
  RHITextureHandle next = 0;
  u32 creates = 0, updates = 0, destroys = 0, target_creates = 0, target_destroys = 0, converts = 0;
  RHITextureHandle last_convert[4] = {};
};

RecordingState g_rec;

struct RHI::Impl {};

RHI::RHI() : impl_(nullptr) {}
RHI::~RHI() {}

bool RHI::Init(const RHIConfig&) { return false; }
void RHI::Shutdown() {}
bool RHI::BeginFrame(Color) { return false; }
void RHI::EndFrame() {}
void RHI::SetScissor(Rect) {}
void RHI::ResetScissor() {}
void RHI::DrawTriangles(const Vertex2D*, u32, const u32*, u32, RHITextureHandle) {}
void RHI::DrawTextTriangles(const Vertex2D*, u32, const u32*, u32, RHITextureHandle) {}

RHITextureHandle RHI::CreateTexture(u32 w, u32 h, RHIFormat format, const void* pixels, RHIFilter) {
  RHITextureHandle id = g_rec.next++;
  size_t bpp = format == RHIFormat::kR8Unorm ? 1 : 4;
  g_rec.textures[id].assign(static_cast<const u8*>(pixels), static_cast<const u8*>(pixels) + w * h * bpp);
  g_rec.widths[id] = w;
  g_rec.heights[id] = h;
  ++g_rec.creates;
  return id;
}
void RHI::UpdateTexture(RHITextureHandle handle, const void* pixels) {
  auto& t = g_rec.textures[handle];
  memcpy(t.data(), pixels, t.size());
  ++g_rec.updates;
}
void RHI::DestroyTexture(RHITextureHandle handle) {
  g_rec.textures.erase(handle);
  ++g_rec.destroys;
}
bool RHI::AcquireFrame() { return false; }
RHITextureHandle RHI::CreateRenderTarget(u32 w, u32 h) {
  RHITextureHandle id = g_rec.next++;
  g_rec.widths[id] = w;
  g_rec.heights[id] = h;
  g_rec.is_target[id] = true;
  ++g_rec.target_creates;
  return id;
}
void RHI::DestroyRenderTarget(RHITextureHandle) { ++g_rec.target_destroys; }
bool RHI::BeginOffscreen(RHITextureHandle, Color) { return false; }
void RHI::EndOffscreen(RHITextureHandle) {}
void RHI::ConvertVideoFrame(RHITextureHandle target, RHITextureHandle y, RHITextureHandle cb, RHITextureHandle cr) {
  g_rec.last_convert[0] = target;
  g_rec.last_convert[1] = y;
  g_rec.last_convert[2] = cb;
  g_rec.last_convert[3] = cr;
  ++g_rec.converts;
}
Vec2 RHI::display_size() const { return {0.0f, 0.0f}; }
f32 RHI::dpi_scale() const { return 1.0f; }

}  // namespace ugui
