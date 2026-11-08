// The scene of aki/ultragui_d3d11/tests/support/draw_data_scene.aki as draw
// data through the C++ ugui_impl_dx11 backend, on a host-owned D3D11 device
// (DXVK-native over Vulkan on Linux) with a GLFW window for its swap chain.
// Compare its screenshot with the aki test's (tools/compare_shots.sh).
// UGUI_SCENE_UNORM=1 gives the host a non-sRGB back buffer, which the backend
// meets with its self-encoding shaders.
//
// ugui/backends/ugui_impl_dx11.cc does not build as it stands (it calls
// std::unordered_map members on ugui::HashMap), so this is built against a copy
// with those four spots changed to HashMap's API; see PORTING.md.
//
//   nix develop -c bash -c 'clang++ -std=c++20 -O1 -I. -I/tmp/ugui-ref-d11/generated \
//     aki/tools/draw_data_scene_dx11.cc /tmp/ugui_impl_dx11_patched.cc \
//     /tmp/ugui-ref-d11/libultragui.a $(pkg-config --libs --cflags dxvk-d3d11 dxvk-dxgi glfw3 freetype2 harfbuzz) \
//     /tmp/ugui-ref-d11/third_party/yoga/yoga/libyogacore.a -ldl -lpthread -o /tmp/draw_data_scene_dx11'
//
// UGUI_SCENE_SECS keeps the window open and redrawing that long.
#define INITGUID
#define CINTERFACE
#define COBJMACROS
#define D3D11_NO_HELPERS
#include <d3d11.h>
#include <dxgi.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ugui/backends/ugui_impl_dx11.h>
#include <ugui/render/draw_data.h>

#include "scene_common.h"

using namespace ugui;
using namespace scene;

int main() {
  if (!glfwInit()) {
    fprintf(stderr, "draw_data_scene_dx11: glfwInit failed\n");
    return 0;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow* win = glfwCreateWindow(640, 400, "draw_data_scene_dx11", nullptr, nullptr);
  if (!win) {
    fprintf(stderr, "draw_data_scene_dx11: no window\n");
    return 0;
  }
  bool unorm = getenv("UGUI_SCENE_UNORM") != nullptr;
  int fb_w = 0, fb_h = 0;
  glfwGetFramebufferSize(win, &fb_w, &fb_h);

  DXGI_SWAP_CHAIN_DESC sc = {};
  sc.BufferDesc.Width = fb_w;
  sc.BufferDesc.Height = fb_h;
  sc.BufferDesc.Format = unorm ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  sc.BufferDesc.RefreshRate.Numerator = 0;
  sc.BufferDesc.RefreshRate.Denominator = 1;
  sc.SampleDesc.Count = 1;
  sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sc.BufferCount = 2;
  sc.OutputWindow = reinterpret_cast<HWND>(win);
  sc.Windowed = TRUE;
  sc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
  D3D_FEATURE_LEVEL got;
  ID3D11Device* device = nullptr;
  ID3D11DeviceContext* ctx = nullptr;
  if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &device, &got,
                               &ctx))) {
    fprintf(stderr, "draw_data_scene_dx11: no D3D11 device\n");
    return 0;
  }
  IDXGIDevice* dxgi_device = nullptr;
  IDXGIAdapter* adapter = nullptr;
  IDXGIFactory* factory = nullptr;
  IDXGISwapChain* swapchain = nullptr;
  ID3D11Device_QueryInterface(device, IID_IDXGIDevice, (void**)&dxgi_device);
  IDXGIDevice_GetAdapter(dxgi_device, &adapter);
  IDXGIAdapter_GetParent(adapter, IID_IDXGIFactory, (void**)&factory);
  if (FAILED(IDXGIFactory_CreateSwapChain(factory, (IUnknown*)device, &sc, &swapchain))) {
    fprintf(stderr, "draw_data_scene_dx11: no swap chain\n");
    return 1;
  }
  IDXGIFactory_Release(factory);
  IDXGIAdapter_Release(adapter);
  IDXGIDevice_Release(dxgi_device);
  ID3D11Texture2D* back = nullptr;
  ID3D11RenderTargetView* rtv = nullptr;
  IDXGISwapChain_GetBuffer(swapchain, 0, IID_ID3D11Texture2D, (void**)&back);
  ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)back, nullptr, &rtv);
  ID3D11Texture2D_Release(back);

  if (!dx11::Init(device, ctx)) return 1;

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
  TextureId c = dx11::CreateTexture(8, 8, RHIFormat::kRgba8Unorm, checker, RHIFilter::kNearest);
  TextureId r = dx11::CreateTexture(4, 4, RHIFormat::kBgra8Unorm, ramp, RHIFilter::kLinear);
  TextureId b = dx11::CreateTexture(32, 24, RHIFormat::kRgba8Unorm, big, RHIFilter::kLinear);

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
    dx11::UpdateFontAtlas(te.atlas_pixels(), static_cast<u32>(sz.x), static_cast<u32>(sz.y));
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
        ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, nullptr);
    const float clear[4] = {0.07f, 0.08f, 0.12f, 1.0f};
    ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, clear);
    dx11::RenderDrawData(dd);
    IDXGISwapChain_Present(swapchain, 1, 0);
    ++frames;
    if (frames >= 3 && glfwGetTime() - start >= hold) break;
    if (glfwWindowShouldClose(win)) break;
  }
  dx11::Shutdown();
  te.Shutdown();
  ID3D11RenderTargetView_Release(rtv);
  IDXGISwapChain_Release(swapchain);
  ID3D11DeviceContext_Release(ctx);
  ID3D11Device_Release(device);
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
