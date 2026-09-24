#ifndef UGUI_BACKENDS_UGUI_IMPL_DX11_H_
#define UGUI_BACKENDS_UGUI_IMPL_DX11_H_

// ugui_impl_dx11: Direct3D 11 renderer backend for ultragui's draw-data mode,
// modeled on Dear ImGui's imgui_impl_dx11.
//
// The HOST owns the device, the swap chain and presentation; this backend
// renders ultragui::DrawData into whatever render target is bound when it is
// called, with shaders and buffers of its own. It saves the pipeline state it
// touches and puts it back, so it can run inside someone else's frame: a game's
// Present hook, say.
//
//   ugui::dx11::Init(device, context);
//   ...
//   const ugui::DrawData& dd = ui.RenderDrawData();
//   ugui::dx11::UpdateFontAtlas(...);   // when atlas_revision() changed
//   ugui::dx11::RenderDrawData(dd);     // into the bound render target
//
// The shaders blend in linear light. On an sRGB render target the hardware
// encodes on write; on any other (the usual game back buffer) the backend
// switches to shaders that encode themselves, so the UI looks the same on
// both.

#include <ugui/core/types.h>
#include <ugui/render/draw_data.h>
#include <ugui/render/texture_backend.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace ugui {
namespace dx11 {

/// Compile shaders and create buffers, states and the white texture on
/// `device`. Takes a reference on both; Shutdown releases them.
bool Init(ID3D11Device* device, ID3D11DeviceContext* context);

/// Release every object the backend created. Call before the device goes.
void Shutdown();

/// (Re)upload the glyph atlas (R8, width*height bytes) as the font texture.
/// Call after Init and whenever TextEngine::atlas_revision() changed.
bool UpdateFontAtlas(const u8* pixels, u32 width, u32 height);

/// Render `draw_data` into the render target bound on the context. Viewport
/// and scissor come from the draw data; everything it changes is restored.
void RenderDrawData(const DrawData& draw_data);

// --- Host texture API (Image/SVG/Lottie/anim in draw-data mode) ---

TextureId CreateTexture(u32 width, u32 height, RHIFormat format,
                        const void* pixels,
                        RHIFilter filter = RHIFilter::kLinear);
void UpdateTexture(TextureId id, const void* pixels);
void DestroyTexture(TextureId id);

/// The backend as a TextureBackend, for UIContext::set_texture_backend().
TextureBackend& texture_backend();

}  // namespace dx11
}  // namespace ugui

#endif  // UGUI_BACKENDS_UGUI_IMPL_DX11_H_
