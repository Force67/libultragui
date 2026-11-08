// The scene of aki/ultragui_scenes/src/draw_data_scene.aki as draw
// data through the C++ ugui_impl_opengl3 backend, on a host-owned GL context.
// Compare its screenshot with the aki test's (tools/compare_shots.sh).
//
//   nix develop -c bash -c 'clang++ -std=c++20 -O1 -I. aki/tools/draw_data_scene.cc \
//     /tmp/ugui-ref-gl/libultragui.a $(pkg-config --libs --cflags glfw3 freetype2 harfbuzz) \
//     -ldl -lpthread -o /tmp/draw_data_scene_gl'
//
// UGUI_SCENE_SECS keeps the window open and redrawing that long.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

#include <ugui/backends/ugui_impl_opengl3.h>
#include <ugui/render/draw_data.h>

#include "scene_common.h"

using namespace ugui;
using namespace scene;

using GLenum = unsigned int;
using GLbitfield = unsigned int;
using GLfloat = float;
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_FRAMEBUFFER_SRGB 0x8DB9

static void (*glEnable_)(GLenum);
static void (*glClearColor_)(GLfloat, GLfloat, GLfloat, GLfloat);
static void (*glClear_)(GLbitfield);

int main() {
  if (!glfwInit()) {
    fprintf(stderr, "draw_data_scene: glfwInit failed\n");
    return 0;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow* win = glfwCreateWindow(640, 400, "draw_data_scene", nullptr, nullptr);
  if (!win) {
    fprintf(stderr, "draw_data_scene: no window\n");
    return 0;
  }
  glfwMakeContextCurrent(win);
  glEnable_ = reinterpret_cast<void (*)(GLenum)>(glfwGetProcAddress("glEnable"));
  glClearColor_ = reinterpret_cast<void (*)(GLfloat, GLfloat, GLfloat, GLfloat)>(glfwGetProcAddress("glClearColor"));
  glClear_ = reinterpret_cast<void (*)(GLbitfield)>(glfwGetProcAddress("glClear"));
  glEnable_(GL_FRAMEBUFFER_SRGB);

  gl::InitInfo info;
  info.get_proc_address = [](const char* n) -> void* { return reinterpret_cast<void*>(glfwGetProcAddress(n)); };
  if (!gl::Init(info)) return 1;

  TextEngine te;
  te.Init(nullptr);
  FontHandle font = te.LoadFont(kFontPath);

  u8 checker[256] = {};
  for (size_t y = 0; y < 8; ++y)
    for (size_t x = 0; x < 8; ++x) {
      bool on = (x + y) % 2 == 0;
      size_t o = (y * 8 + x) * 4;
      checker[o] = on ? 240 : 40;
      checker[o + 1] = on ? 200 : 60;
      checker[o + 2] = on ? 60 : 160;
      checker[o + 3] = 255;
    }
  u8 ramp[64] = {};
  for (size_t y = 0; y < 4; ++y)
    for (size_t x = 0; x < 4; ++x) {
      size_t o = (y * 4 + x) * 4;
      ramp[o] = static_cast<u8>(x * 85);
      ramp[o + 1] = static_cast<u8>(y * 85);
      ramp[o + 2] = static_cast<u8>(255 - x * 60);
      ramp[o + 3] = 255;
    }
  u8 big[3072] = {};
  for (size_t y = 0; y < 24; ++y)
    for (size_t x = 0; x < 32; ++x) {
      size_t o = (y * 32 + x) * 4;
      big[o] = static_cast<u8>(x * 8);
      big[o + 1] = static_cast<u8>(y * 10);
      big[o + 2] = static_cast<u8>(200 - x * 4);
      big[o + 3] = 255;
    }
  TextureId c = gl::CreateTexture(8, 8, RHIFormat::kRgba8Unorm, checker, RHIFilter::kNearest);
  TextureId r = gl::CreateTexture(4, 4, RHIFormat::kBgra8Unorm, ramp, RHIFilter::kLinear);
  TextureId b = gl::CreateTexture(32, 24, RHIFormat::kRgba8Unorm, big, RHIFilter::kLinear);

  const char* env = getenv("UGUI_SCENE_SECS");
  double hold = env ? atof(env) : 0.0;
  double start = glfwGetTime();
  int frames = 0;
  while (true) {
    glfwPollEvents();
    te.BeginFrame();
    Rect full = {0, 0, 640, 400};
    Geometry quads, text;
    BuildMain(quads, text, te, font, RhiHandleFromTextureId(c), RhiHandleFromTextureId(r), RhiHandleFromTextureId(b), full);
    std::vector<DrawCmd> cmds;
    for (const Batch& bt : quads.batches) {
      DrawCmd cmd;
      cmd.clip_rect = bt.scissor;
      cmd.texture_id = TextureIdFromRhiHandle(bt.texture);
      cmd.index_offset = bt.index_offset;
      cmd.elem_count = bt.index_count;
      cmd.is_text = false;
      cmds.push_back(cmd);
    }
    for (const Batch& bt : text.batches) {
      DrawCmd cmd;
      cmd.clip_rect = bt.scissor;
      cmd.texture_id = kFontTextureId;
      cmd.index_offset = bt.index_offset;
      cmd.elem_count = bt.index_count;
      cmd.is_text = true;
      cmds.push_back(cmd);
    }
    Vec2 sz = te.atlas_size();
    gl::UpdateFontAtlas(te.atlas_pixels(), static_cast<u32>(sz.x), static_cast<u32>(sz.y));
    DrawData dd;
    dd.quad_vertices = quads.vertices.data();
    dd.quad_vertex_count = static_cast<u32>(quads.vertices.size());
    dd.quad_indices = quads.indices.data();
    dd.quad_index_count = static_cast<u32>(quads.indices.size());
    dd.text_vertices = text.vertices.data();
    dd.text_vertex_count = static_cast<u32>(text.vertices.size());
    dd.text_indices = text.indices.data();
    dd.text_index_count = static_cast<u32>(text.indices.size());
    dd.commands = cmds.data();
    dd.command_count = static_cast<u32>(cmds.size());
    dd.display_size = {640, 400};
    dd.valid = true;
    gl::NewFrame();
    glClearColor_(0.07f, 0.08f, 0.12f, 1.0f);
    glClear_(GL_COLOR_BUFFER_BIT);
    gl::RenderDrawData(dd);
    glfwSwapBuffers(win);
    ++frames;
    if (frames >= 3 && glfwGetTime() - start >= hold) break;
    if (glfwWindowShouldClose(win)) break;
  }
  gl::Shutdown();
  te.Shutdown();
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
