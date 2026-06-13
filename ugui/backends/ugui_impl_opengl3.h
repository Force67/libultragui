#ifndef UGUI_BACKENDS_UGUI_IMPL_OPENGL3_H_
#define UGUI_BACKENDS_UGUI_IMPL_OPENGL3_H_

// ugui_impl_opengl3: OpenGL 3.3 core renderer backend for ultragui, modeled on
// Dear ImGui's imgui_impl_opengl3.
//
// The HOST owns the window, GL context and presentation; this backend only
// renders ultragui::DrawData into the currently bound framebuffer using its own
// shaders and buffers. Typical per-frame use inside your own GL render loop:
//
//   ugui::gl::NewFrame();
//   const ugui::DrawData& dd = ui.RenderDrawData();
//   // ... your scene rendering ...
//   ugui::gl::RenderDrawData(dd);   // ui on top, into the bound framebuffer
//   glfwSwapBuffers(window);        // you present
//
// Font atlas: call UpdateFontAtlas() whenever TextEngine::atlas_revision()
// changes, passing TextEngine::atlas_pixels()/atlas_size().

#include <ugui/core/types.h>
#include <ugui/render/draw_data.h>
#include <ugui/render/texture_backend.h>

namespace ugui {
namespace gl {

struct InitInfo {
  /// GL function loader, e.g. glfwGetProcAddress. The backend resolves the core
  /// 3.3 entry points it needs through this (it links no GL itself).
  void* (*get_proc_address)(const char* name) = nullptr;
};

/// Compile shaders, create the VAO/buffers and the white fallback texture.
bool Init(const InitInfo& info);

/// Release all backend GL objects (does not touch the host's GL context).
void Shutdown();

/// Call once at the start of each frame, before RenderDrawData.
void NewFrame();

/// (Re)upload the glyph atlas (R8/alpha8, width*height bytes) as the font
/// texture. Call from Init and whenever TextEngine::atlas_revision() changed.
bool UpdateFontAtlas(const u8* pixels, u32 width, u32 height);

/// Render `draw_data` into the currently bound framebuffer. Sets the GL state
/// it needs (blend, scissor, no depth/cull); restore your own state afterwards
/// if you render more.
void RenderDrawData(const DrawData& draw_data);

// --- Host texture API (for Image/SVG/Lottie/anim in draw-data mode) ---
//
// Create textures the backend can bind from draw commands. Hand the returned
// TextureId to ultragui via UIContext::set_texture_backend(). Byte formats
// (R8/RGBA8/BGRA8) only; the GL backend does not handle float textures.

TextureId CreateTexture(u32 width, u32 height, RHIFormat format,
                        const void* pixels,
                        RHIFilter filter = RHIFilter::kLinear);
void UpdateTexture(TextureId id, const void* pixels);
void DestroyTexture(TextureId id);

/// The backend as a TextureBackend, for UIContext::set_texture_backend().
TextureBackend& texture_backend();

}  // namespace gl
}  // namespace ugui

#endif  // UGUI_BACKENDS_UGUI_IMPL_OPENGL3_H_
