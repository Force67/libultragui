// ugui_impl_dx11: see ugui_impl_dx11.h.
//
// C-style COM, as the D3D11 RHI uses: DXVK-native exposes the C vtables, where
// the C++ ABI can disagree, and the Windows SDK offers the same behind
// CINTERFACE, so one call style builds on both.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define CINTERFACE
#define COBJMACROS
// d3d11.h's C++ helper classes call members that CINTERFACE takes away.
#define D3D11_NO_HELPERS

#include <d3d11.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <cstdio>
#include <cstring>
#include <ugui/backends/ugui_impl_dx11.h>
#include <ugui/rhi/d3d11/d3d_shader_compiler.h>

// kQuadHlsl / kTextHlsl, generated from shaders/hlsl/*.hlsl by
// cmake/EmbedHlsl.cmake.
#include <ugui_hlsl_embedded.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

namespace ugui {
namespace dx11 {
namespace {

template <typename T>
void SafeRelease(T*& object) {
  if (object) {
    object->lpVtbl->Release(object);
    object = nullptr;
  }
}

struct Texture {
  ID3D11Texture2D* texture = nullptr;
  ID3D11ShaderResourceView* srv = nullptr;
  u32 width = 0;
  u32 height = 0;
  u32 pixel_size = 4;
  RHIFilter filter = RHIFilter::kLinear;
};

// The two pipelines' pixel shaders, for an sRGB target and for any other.
struct PixelShaders {
  ID3D11PixelShader* quad = nullptr;
  ID3D11PixelShader* text = nullptr;
};

// One vertex and index buffer pair, grown as the UI needs.
struct Geometry {
  ID3D11Buffer* vertices = nullptr;
  u32 vertex_capacity = 0;
  ID3D11Buffer* indices = nullptr;
  u32 index_capacity = 0;
};

struct Backend {
  ID3D11Device* device = nullptr;
  ID3D11DeviceContext* context = nullptr;
  ID3D11VertexShader* quad_vs = nullptr;
  ID3D11VertexShader* text_vs = nullptr;
  PixelShaders linear_target;  // for an sRGB render target
  PixelShaders encoding;       // for any other; compiled when first needed
  bool encoding_failed = false;
  ID3D11InputLayout* layout = nullptr;
  ID3D11Buffer* projection = nullptr;
  ID3D11BlendState* blend = nullptr;
  ID3D11RasterizerState* raster = nullptr;
  ID3D11DepthStencilState* depth = nullptr;
  ID3D11SamplerState* linear = nullptr;
  ID3D11SamplerState* nearest = nullptr;
  Texture white;
  Texture font;
  Geometry quads;
  Geometry text;
  HashMap<TextureId, Texture> textures;
  TextureId next_texture = 1;
};

Backend g;

bool Check(HRESULT hr, const char* what) {
  if (SUCCEEDED(hr)) return true;
  std::fprintf(stderr, "ugui_impl_dx11: %s failed: 0x%08lx\n", what,
               static_cast<unsigned long>(hr));
  return false;
}

bool FormatOf(RHIFormat format, DXGI_FORMAT& dxgi, u32& pixel_size) {
  switch (format) {
    case RHIFormat::kRgba8Unorm:
      dxgi = DXGI_FORMAT_R8G8B8A8_UNORM;
      pixel_size = 4;
      return true;
    case RHIFormat::kBgra8Unorm:
      dxgi = DXGI_FORMAT_B8G8R8A8_UNORM;
      pixel_size = 4;
      return true;
    case RHIFormat::kR8Unorm:
      dxgi = DXGI_FORMAT_R8_UNORM;
      pixel_size = 1;
      return true;
    default:
      return false;
  }
}

bool MakeTexture(u32 width, u32 height, RHIFormat format, const void* pixels,
                 RHIFilter filter, Texture& out) {
  DXGI_FORMAT dxgi = DXGI_FORMAT_UNKNOWN;
  u32 pixel_size = 0;
  if (width == 0 || height == 0 || !FormatOf(format, dxgi, pixel_size))
    return false;
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = dxgi;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA data = {};
  data.pSysMem = pixels;
  data.SysMemPitch = width * pixel_size;
  Texture made;
  if (!Check(ID3D11Device_CreateTexture2D(g.device, &desc,
                                          pixels ? &data : nullptr,
                                          &made.texture),
             "CreateTexture2D"))
    return false;
  if (!Check(ID3D11Device_CreateShaderResourceView(
                 g.device, (ID3D11Resource*)made.texture, nullptr, &made.srv),
             "CreateShaderResourceView")) {
    SafeRelease(made.texture);
    return false;
  }
  made.width = width;
  made.height = height;
  made.pixel_size = pixel_size;
  made.filter = filter;
  out = made;
  return true;
}

void FreeTexture(Texture& texture) {
  SafeRelease(texture.srv);
  SafeRelease(texture.texture);
}

bool MakeVertexShader(const char* source, ID3D11VertexShader** out,
                      bool with_layout) {
  Vector<char> code;
  if (!d3d::CompileHlsl(source, "VSMain", "vs_5_0", code)) return false;
  if (!Check(ID3D11Device_CreateVertexShader(g.device, code.data(), code.size(),
                                             nullptr, out),
             "CreateVertexShader"))
    return false;
  if (!with_layout) return true;
  // Vertex2D, as the quad and text shaders read it.
  const D3D11_INPUT_ELEMENT_DESC layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32_UINT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 1, DXGI_FORMAT_R32_UINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"BLENDINDICES", 0, DXGI_FORMAT_R32_UINT, 0, 24,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"BLENDWEIGHT", 0, DXGI_FORMAT_R32_FLOAT, 0, 28,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 32,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"BLENDWEIGHT", 1, DXGI_FORMAT_R32_FLOAT, 0, 40,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 2, DXGI_FORMAT_R32_UINT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  return Check(ID3D11Device_CreateInputLayout(
                   g.device, layout, sizeof(layout) / sizeof(layout[0]),
                   code.data(), code.size(), &g.layout),
               "CreateInputLayout");
}

bool MakePixelShaders(bool encode, PixelShaders& out) {
  const char* const defines[] = {"UGUI_ENCODE_SRGB", nullptr};
  const char* const* use = encode ? defines : nullptr;
  Vector<char> code;
  if (!d3d::CompileHlsl(kQuadHlsl, "PSMain", "ps_5_0", code, use) ||
      !Check(ID3D11Device_CreatePixelShader(g.device, code.data(), code.size(),
                                            nullptr, &out.quad),
             "CreatePixelShader"))
    return false;
  if (!d3d::CompileHlsl(kTextHlsl, "PSMain", "ps_5_0", code, use) ||
      !Check(ID3D11Device_CreatePixelShader(g.device, code.data(), code.size(),
                                            nullptr, &out.text),
             "CreatePixelShader")) {
    SafeRelease(out.quad);
    return false;
  }
  return true;
}

bool MakeStates() {
  D3D11_BUFFER_DESC cb = {};
  cb.ByteWidth = 16;  // scale.xy, translate.xy
  cb.Usage = D3D11_USAGE_DYNAMIC;
  cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (!Check(ID3D11Device_CreateBuffer(g.device, &cb, nullptr, &g.projection),
             "CreateBuffer (projection)"))
    return false;

  D3D11_BLEND_DESC bd = {};
  bd.RenderTarget[0].BlendEnable = TRUE;
  bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  if (!Check(ID3D11Device_CreateBlendState(g.device, &bd, &g.blend),
             "CreateBlendState"))
    return false;

  D3D11_RASTERIZER_DESC rd = {};
  rd.FillMode = D3D11_FILL_SOLID;
  rd.CullMode = D3D11_CULL_NONE;
  rd.ScissorEnable = TRUE;
  rd.DepthClipEnable = TRUE;
  if (!Check(ID3D11Device_CreateRasterizerState(g.device, &rd, &g.raster),
             "CreateRasterizerState"))
    return false;

  // The host may leave a depth buffer bound; the UI ignores it.
  D3D11_DEPTH_STENCIL_DESC dd = {};
  dd.DepthEnable = FALSE;
  dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
  dd.StencilEnable = FALSE;
  dd.FrontFace.StencilFailOp = dd.FrontFace.StencilDepthFailOp =
      dd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  dd.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  dd.BackFace = dd.FrontFace;
  if (!Check(ID3D11Device_CreateDepthStencilState(g.device, &dd, &g.depth),
             "CreateDepthStencilState"))
    return false;

  D3D11_SAMPLER_DESC sd = {};
  sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.MaxAnisotropy = 1;
  sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sd.MaxLOD = D3D11_FLOAT32_MAX;
  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  if (!Check(ID3D11Device_CreateSamplerState(g.device, &sd, &g.linear),
             "CreateSamplerState (linear)"))
    return false;
  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
  return Check(ID3D11Device_CreateSamplerState(g.device, &sd, &g.nearest),
               "CreateSamplerState (nearest)");
}

// Makes room for `count` elements of `stride` bytes, doubling as it grows.
bool Reserve(ID3D11Buffer*& buffer, u32& capacity, u32 count, u32 stride,
             UINT bind) {
  if (buffer && capacity >= count) return true;
  u32 grown = capacity > 0 ? capacity : 1024;
  while (grown < count) grown *= 2;
  SafeRelease(buffer);
  capacity = 0;
  D3D11_BUFFER_DESC desc = {};
  desc.ByteWidth = grown * stride;
  desc.Usage = D3D11_USAGE_DYNAMIC;
  desc.BindFlags = bind;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (!Check(ID3D11Device_CreateBuffer(g.device, &desc, nullptr, &buffer),
             "CreateBuffer"))
    return false;
  capacity = grown;
  return true;
}

bool Upload(ID3D11Buffer* buffer, const void* data, u32 bytes) {
  if (bytes == 0) return true;
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  if (!Check(ID3D11DeviceContext_Map(g.context, (ID3D11Resource*)buffer, 0,
                                     D3D11_MAP_WRITE_DISCARD, 0, &mapped),
             "Map"))
    return false;
  std::memcpy(mapped.pData, data, bytes);
  ID3D11DeviceContext_Unmap(g.context, (ID3D11Resource*)buffer, 0);
  return true;
}

bool UploadGeometry(Geometry& geometry, const Vertex2D* vertices,
                    u32 vertex_count, const u32* indices, u32 index_count) {
  if (vertex_count == 0 || index_count == 0) return true;
  return Reserve(geometry.vertices, geometry.vertex_capacity, vertex_count,
                 sizeof(Vertex2D), D3D11_BIND_VERTEX_BUFFER) &&
         Reserve(geometry.indices, geometry.index_capacity, index_count,
                 sizeof(u32), D3D11_BIND_INDEX_BUFFER) &&
         Upload(geometry.vertices, vertices, vertex_count * sizeof(Vertex2D)) &&
         Upload(geometry.indices, indices, index_count * sizeof(u32));
}

// Whether the bound render target encodes sRGB on write. Null when nothing
// is bound, and then there is nowhere to draw.
bool BoundTarget(bool& srgb) {
  ID3D11RenderTargetView* rtv = nullptr;
  ID3D11DeviceContext_OMGetRenderTargets(g.context, 1, &rtv, nullptr);
  if (!rtv) return false;
  D3D11_RENDER_TARGET_VIEW_DESC desc = {};
  ID3D11RenderTargetView_GetDesc(rtv, &desc);
  ID3D11RenderTargetView_Release(rtv);
  switch (desc.Format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      srgb = true;
      break;
    default:
      srgb = false;
      break;
  }
  return true;
}

// Everything RenderDrawData changes, as the host had it.
struct SavedState {
  static constexpr UINT kMaxRects =
      D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  UINT scissor_count = kMaxRects;
  UINT viewport_count = kMaxRects;
  D3D11_RECT scissors[kMaxRects] = {};
  D3D11_VIEWPORT viewports[kMaxRects] = {};
  ID3D11RasterizerState* raster = nullptr;
  ID3D11BlendState* blend = nullptr;
  FLOAT blend_factor[4] = {};
  UINT sample_mask = 0;
  ID3D11DepthStencilState* depth = nullptr;
  UINT stencil_ref = 0;
  ID3D11ShaderResourceView* ps_srv = nullptr;
  ID3D11SamplerState* ps_samplers[2] = {};
  ID3D11PixelShader* ps = nullptr;
  ID3D11VertexShader* vs = nullptr;
  ID3D11GeometryShader* gs = nullptr;
  ID3D11HullShader* hs = nullptr;
  ID3D11DomainShader* ds = nullptr;
  ID3D11ClassInstance* ps_instances[256] = {};
  ID3D11ClassInstance* vs_instances[256] = {};
  ID3D11ClassInstance* gs_instances[256] = {};
  UINT ps_instance_count = 256;
  UINT vs_instance_count = 256;
  UINT gs_instance_count = 256;
  ID3D11Buffer* vs_constants = nullptr;
  D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
  ID3D11Buffer* index_buffer = nullptr;
  DXGI_FORMAT index_format = DXGI_FORMAT_UNKNOWN;
  UINT index_offset = 0;
  ID3D11Buffer* vertex_buffer = nullptr;
  UINT vertex_stride = 0;
  UINT vertex_offset = 0;
  ID3D11InputLayout* layout = nullptr;
};

void Save(ID3D11DeviceContext* c, SavedState& s) {
  ID3D11DeviceContext_RSGetScissorRects(c, &s.scissor_count, s.scissors);
  ID3D11DeviceContext_RSGetViewports(c, &s.viewport_count, s.viewports);
  ID3D11DeviceContext_RSGetState(c, &s.raster);
  ID3D11DeviceContext_OMGetBlendState(c, &s.blend, s.blend_factor,
                                      &s.sample_mask);
  ID3D11DeviceContext_OMGetDepthStencilState(c, &s.depth, &s.stencil_ref);
  ID3D11DeviceContext_PSGetShaderResources(c, 0, 1, &s.ps_srv);
  ID3D11DeviceContext_PSGetSamplers(c, 0, 2, s.ps_samplers);
  ID3D11DeviceContext_PSGetShader(c, &s.ps, s.ps_instances,
                                  &s.ps_instance_count);
  ID3D11DeviceContext_VSGetShader(c, &s.vs, s.vs_instances,
                                  &s.vs_instance_count);
  ID3D11DeviceContext_GSGetShader(c, &s.gs, s.gs_instances,
                                  &s.gs_instance_count);
  ID3D11DeviceContext_HSGetShader(c, &s.hs, nullptr, nullptr);
  ID3D11DeviceContext_DSGetShader(c, &s.ds, nullptr, nullptr);
  ID3D11DeviceContext_VSGetConstantBuffers(c, 0, 1, &s.vs_constants);
  ID3D11DeviceContext_IAGetPrimitiveTopology(c, &s.topology);
  ID3D11DeviceContext_IAGetIndexBuffer(c, &s.index_buffer, &s.index_format,
                                       &s.index_offset);
  ID3D11DeviceContext_IAGetVertexBuffers(c, 0, 1, &s.vertex_buffer,
                                         &s.vertex_stride, &s.vertex_offset);
  ID3D11DeviceContext_IAGetInputLayout(c, &s.layout);
}

void Restore(ID3D11DeviceContext* c, SavedState& s) {
  ID3D11DeviceContext_RSSetScissorRects(c, s.scissor_count, s.scissors);
  ID3D11DeviceContext_RSSetViewports(c, s.viewport_count, s.viewports);
  ID3D11DeviceContext_RSSetState(c, s.raster);
  ID3D11DeviceContext_OMSetBlendState(c, s.blend, s.blend_factor,
                                      s.sample_mask);
  ID3D11DeviceContext_OMSetDepthStencilState(c, s.depth, s.stencil_ref);
  ID3D11DeviceContext_PSSetShaderResources(c, 0, 1, &s.ps_srv);
  ID3D11DeviceContext_PSSetSamplers(c, 0, 2, s.ps_samplers);
  ID3D11DeviceContext_PSSetShader(c, s.ps, s.ps_instances, s.ps_instance_count);
  ID3D11DeviceContext_VSSetShader(c, s.vs, s.vs_instances, s.vs_instance_count);
  ID3D11DeviceContext_GSSetShader(c, s.gs, s.gs_instances, s.gs_instance_count);
  ID3D11DeviceContext_HSSetShader(c, s.hs, nullptr, 0);
  ID3D11DeviceContext_DSSetShader(c, s.ds, nullptr, 0);
  ID3D11DeviceContext_VSSetConstantBuffers(c, 0, 1, &s.vs_constants);
  ID3D11DeviceContext_IASetPrimitiveTopology(c, s.topology);
  ID3D11DeviceContext_IASetIndexBuffer(c, s.index_buffer, s.index_format,
                                       s.index_offset);
  ID3D11DeviceContext_IASetVertexBuffers(c, 0, 1, &s.vertex_buffer,
                                         &s.vertex_stride, &s.vertex_offset);
  ID3D11DeviceContext_IASetInputLayout(c, s.layout);

  // The getters each took a reference.
  SafeRelease(s.raster);
  SafeRelease(s.blend);
  SafeRelease(s.depth);
  SafeRelease(s.ps_srv);
  SafeRelease(s.ps_samplers[0]);
  SafeRelease(s.ps_samplers[1]);
  SafeRelease(s.ps);
  SafeRelease(s.vs);
  SafeRelease(s.gs);
  SafeRelease(s.hs);
  SafeRelease(s.ds);
  for (UINT i = 0; i < s.ps_instance_count; ++i) SafeRelease(s.ps_instances[i]);
  for (UINT i = 0; i < s.vs_instance_count; ++i) SafeRelease(s.vs_instances[i]);
  for (UINT i = 0; i < s.gs_instance_count; ++i) SafeRelease(s.gs_instances[i]);
  SafeRelease(s.vs_constants);
  SafeRelease(s.index_buffer);
  SafeRelease(s.vertex_buffer);
  SafeRelease(s.layout);
}

void BindGeometry(const Geometry& geometry) {
  const UINT stride = sizeof(Vertex2D);
  const UINT offset = 0;
  ID3D11DeviceContext_IASetVertexBuffers(g.context, 0, 1, &geometry.vertices,
                                         &stride, &offset);
  ID3D11DeviceContext_IASetIndexBuffer(g.context, geometry.indices,
                                       DXGI_FORMAT_R32_UINT, 0);
}

const Texture* TextureFor(const DrawCmd& command) {
  if (command.is_text || command.texture_id == kFontTextureId)
    return g.font.srv ? &g.font : &g.white;
  if (command.texture_id == kNullTextureId) return &g.white;
  auto it = g.textures.find(command.texture_id);
  return it != g.textures.end() ? &it->second : &g.white;
}

struct TextureSink final : TextureBackend {
  TextureId CreateTexture(u32 width, u32 height, RHIFormat format,
                          const void* pixels, RHIFilter filter) override {
    return dx11::CreateTexture(width, height, format, pixels, filter);
  }
  void UpdateTexture(TextureId id, const void* pixels) override {
    dx11::UpdateTexture(id, pixels);
  }
  void DestroyTexture(TextureId id) override { dx11::DestroyTexture(id); }
};

TextureSink g_sink;

}  // namespace

bool Init(ID3D11Device* device, ID3D11DeviceContext* context) {
  if (g.device) Shutdown();
  if (!device || !context) return false;
  g.device = device;
  g.context = context;
  ID3D11Device_AddRef(device);
  ID3D11DeviceContext_AddRef(context);
  const u8 white_pixel[4] = {255, 255, 255, 255};
  if (!MakeVertexShader(kQuadHlsl, &g.quad_vs, true) ||
      !MakeVertexShader(kTextHlsl, &g.text_vs, false) ||
      !MakePixelShaders(false, g.linear_target) || !MakeStates() ||
      !MakeTexture(1, 1, RHIFormat::kRgba8Unorm, white_pixel,
                   RHIFilter::kLinear, g.white)) {
    Shutdown();
    return false;
  }
  return true;
}

void Shutdown() {
  for (auto& entry : g.textures) FreeTexture(entry.second);
  g.textures.clear();
  FreeTexture(g.white);
  FreeTexture(g.font);
  SafeRelease(g.quads.vertices);
  SafeRelease(g.quads.indices);
  SafeRelease(g.text.vertices);
  SafeRelease(g.text.indices);
  SafeRelease(g.quad_vs);
  SafeRelease(g.text_vs);
  SafeRelease(g.linear_target.quad);
  SafeRelease(g.linear_target.text);
  SafeRelease(g.encoding.quad);
  SafeRelease(g.encoding.text);
  SafeRelease(g.layout);
  SafeRelease(g.projection);
  SafeRelease(g.blend);
  SafeRelease(g.raster);
  SafeRelease(g.depth);
  SafeRelease(g.linear);
  SafeRelease(g.nearest);
  SafeRelease(g.context);
  SafeRelease(g.device);
  g = Backend();
}

bool UpdateFontAtlas(const u8* pixels, u32 width, u32 height) {
  if (!g.device || !pixels) return false;
  if (g.font.texture && g.font.width == width && g.font.height == height) {
    ID3D11DeviceContext_UpdateSubresource(g.context,
                                          (ID3D11Resource*)g.font.texture, 0,
                                          nullptr, pixels, width, 0);
    return true;
  }
  FreeTexture(g.font);
  return MakeTexture(width, height, RHIFormat::kR8Unorm, pixels,
                     RHIFilter::kNearest, g.font);
}

void RenderDrawData(const DrawData& dd) {
  if (!g.device || !dd.valid || dd.command_count == 0) return;
  if (dd.display_size.x <= 0.0f || dd.display_size.y <= 0.0f) return;
  bool srgb = false;
  if (!BoundTarget(srgb)) return;
  if (!srgb && !g.encoding.quad && !g.encoding_failed)
    g.encoding_failed = !MakePixelShaders(true, g.encoding);
  const PixelShaders& ps =
      srgb || g.encoding_failed ? g.linear_target : g.encoding;

  if (!UploadGeometry(g.quads, dd.quad_vertices, dd.quad_vertex_count,
                      dd.quad_indices, dd.quad_index_count) ||
      !UploadGeometry(g.text, dd.text_vertices, dd.text_vertex_count,
                      dd.text_indices, dd.text_index_count))
    return;

  // Display coordinates to clip space, y down.
  const f32 projection[4] = {2.0f / dd.display_size.x,
                             -2.0f / dd.display_size.y, -1.0f, 1.0f};
  if (!Upload(g.projection, projection, sizeof(projection))) return;

  SavedState saved;
  Save(g.context, saved);

  const f32 sx = dd.framebuffer_scale.x > 0 ? dd.framebuffer_scale.x : 1.0f;
  const f32 sy = dd.framebuffer_scale.y > 0 ? dd.framebuffer_scale.y : 1.0f;
  D3D11_VIEWPORT viewport = {};
  viewport.Width = dd.display_size.x * sx;
  viewport.Height = dd.display_size.y * sy;
  viewport.MaxDepth = 1.0f;
  ID3D11DeviceContext_RSSetViewports(g.context, 1, &viewport);
  ID3D11DeviceContext_RSSetState(g.context, g.raster);
  const f32 blend_factor[4] = {};
  ID3D11DeviceContext_OMSetBlendState(g.context, g.blend, blend_factor,
                                      0xffffffff);
  ID3D11DeviceContext_OMSetDepthStencilState(g.context, g.depth, 0);
  ID3D11DeviceContext_IASetInputLayout(g.context, g.layout);
  ID3D11DeviceContext_IASetPrimitiveTopology(
      g.context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ID3D11DeviceContext_VSSetConstantBuffers(g.context, 0, 1, &g.projection);
  ID3D11DeviceContext_GSSetShader(g.context, nullptr, nullptr, 0);
  ID3D11DeviceContext_HSSetShader(g.context, nullptr, nullptr, 0);
  ID3D11DeviceContext_DSSetShader(g.context, nullptr, nullptr, 0);
  // The quad shader samples s0, the text shader s1.
  ID3D11SamplerState* samplers[2] = {g.linear, g.nearest};
  ID3D11DeviceContext_PSSetSamplers(g.context, 0, 2, samplers);

  int bound = -1;  // 0 quads, 1 text
  for (u32 i = 0; i < dd.command_count; ++i) {
    const DrawCmd& c = dd.commands[i];
    if (c.elem_count == 0) continue;
    // Backdrop blur wants a blurred copy of the scene, which this backend
    // does not keep; the command is a stand-in quad, left out.
    if (c.blur != 0.0f) continue;

    const int kind = c.is_text ? 1 : 0;
    if (kind != bound) {
      bound = kind;
      BindGeometry(kind ? g.text : g.quads);
      ID3D11DeviceContext_VSSetShader(g.context, kind ? g.text_vs : g.quad_vs,
                                      nullptr, 0);
      ID3D11DeviceContext_PSSetShader(g.context, kind ? ps.text : ps.quad,
                                      nullptr, 0);
    }
    const Texture* texture = TextureFor(c);
    ID3D11DeviceContext_PSSetShaderResources(g.context, 0, 1, &texture->srv);
    if (!c.is_text) {
      ID3D11SamplerState* sampler =
          texture->filter == RHIFilter::kNearest ? g.nearest : g.linear;
      ID3D11DeviceContext_PSSetSamplers(g.context, 0, 1, &sampler);
    }

    D3D11_RECT scissor;
    scissor.left = static_cast<LONG>(c.clip_rect.x * sx);
    scissor.top = static_cast<LONG>(c.clip_rect.y * sy);
    scissor.right = static_cast<LONG>((c.clip_rect.x + c.clip_rect.w) * sx);
    scissor.bottom = static_cast<LONG>((c.clip_rect.y + c.clip_rect.h) * sy);
    if (scissor.left < 0) scissor.left = 0;
    if (scissor.top < 0) scissor.top = 0;
    if (scissor.right <= scissor.left || scissor.bottom <= scissor.top)
      continue;
    ID3D11DeviceContext_RSSetScissorRects(g.context, 1, &scissor);
    ID3D11DeviceContext_DrawIndexed(g.context, c.elem_count, c.index_offset,
                                    0);
  }

  Restore(g.context, saved);
}

TextureId CreateTexture(u32 width, u32 height, RHIFormat format,
                        const void* pixels, RHIFilter filter) {
  if (!g.device) return kNullTextureId;
  Texture texture;
  if (!MakeTexture(width, height, format, pixels, filter, texture))
    return kNullTextureId;
  TextureId id = g.next_texture++;
  if (id == kFontTextureId) id = g.next_texture++;
  g.textures.emplace(id, texture);
  return id;
}

void UpdateTexture(TextureId id, const void* pixels) {
  auto it = g.textures.find(id);
  if (it == g.textures.end() || !pixels) return;
  const Texture& t = it->second;
  ID3D11DeviceContext_UpdateSubresource(g.context, (ID3D11Resource*)t.texture,
                                        0, nullptr, pixels,
                                        t.width * t.pixel_size, 0);
}

void DestroyTexture(TextureId id) {
  auto it = g.textures.find(id);
  if (it == g.textures.end()) return;
  FreeTexture(it->second);
  g.textures.erase(it);
}

TextureBackend& texture_backend() { return g_sink; }

}  // namespace dx11
}  // namespace ugui

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
