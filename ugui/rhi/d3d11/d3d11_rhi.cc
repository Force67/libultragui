#include <ugui/platform/platform.h>
#include <ugui/rhi/rhi.h>

// Use C-style COM interface (DXVK-native is a C library; C++ vtable ABI can
// mismatch)
#define NOMINMAX
#define INITGUID
#define CINTERFACE
#define COBJMACROS

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

// Suppress any min/max macros
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

namespace ugui {

// ---------------------------------------------------------------------------
// Embedded HLSL shaders (compiled at runtime via D3DCompile)
// ---------------------------------------------------------------------------

static const char* quad_hlsl = R"hlsl(
// ultragui quad shader -- D3D12 HLSL port of quad.vert + quad.frag
// Compile: dxc -T vs_6_0 -E VSMain -Fo quad_vs.cso quad.hlsl
//          dxc -T ps_6_0 -E PSMain -Fo quad_ps.cso quad.hlsl

cbuffer PushConstants : register(b0) {
    float2 scale;
    float2 translate;
};

Texture2D tex : register(t0);
SamplerState samp_linear : register(s0);

struct VSInput {
    float2 pos          : POSITION;
    float2 uv           : TEXCOORD0;
    uint   color        : COLOR0;
    uint   color2       : COLOR1;
    uint   corner_radii : BLENDINDICES0;
    float  softness     : BLENDWEIGHT0;
    float2 half_size    : TEXCOORD1;
    float  border_width : BLENDWEIGHT1;
    uint   border_color : COLOR2;
};

struct VSOutput {
    float4 pos          : SV_Position;
    float2 uv           : TEXCOORD0;
    float4 color        : COLOR0;
    float4 color2       : COLOR1;
    float4 corner_radii : TEXCOORD1;
    float  softness     : TEXCOORD2;
    float2 half_size    : TEXCOORD3;
    float  border_width : TEXCOORD4;
    float4 border_color : COLOR2;
};

// sRGB EOTF: sRGB -> linear
float3 srgb_to_linear(float3 c) {
    return lerp(c / 12.92, pow((c + 0.055) / 1.055, 2.4), step(0.04045, c));
}

float4 unpack_color(uint c) {
    float4 col = float4(
        float(c & 0xFFu) / 255.0,
        float((c >> 8u) & 0xFFu) / 255.0,
        float((c >> 16u) & 0xFFu) / 255.0,
        float((c >> 24u) & 0xFFu) / 255.0
    );
    col.rgb = srgb_to_linear(col.rgb);
    return col;
}

VSOutput VSMain(VSInput input) {
    VSOutput o;
    o.pos = float4(input.pos * scale + translate, 0.0, 1.0);
    o.uv = input.uv;
    o.color = unpack_color(input.color);
    o.color2 = unpack_color(input.color2);
    o.corner_radii = float4(
        float(input.corner_radii & 0xFFu),
        float((input.corner_radii >> 8u) & 0xFFu),
        float((input.corner_radii >> 16u) & 0xFFu),
        float((input.corner_radii >> 24u) & 0xFFu)
    );
    o.softness = input.softness;
    o.half_size = input.half_size;
    o.border_width = input.border_width;
    o.border_color = unpack_color(input.border_color);
    return o;
}

float sdf_rounded_rect_4(float2 p, float2 b, float4 radii) {
    // radii = (tl, tr, br, bl)
    float radius = (p.x > 0.0) ? ((p.y > 0.0) ? radii.z : radii.y) : ((p.y > 0.0) ? radii.w : radii.x);
    float2 q = abs(p) - b + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float4 PSMain(VSOutput input) : SV_Target {
    float4 tex_color = tex.Sample(samp_linear, input.uv);
    tex_color.rgb = srgb_to_linear(tex_color.rgb);

    // Linear gradient along V axis
    float4 base_color = lerp(input.color, input.color2, input.uv.y);
    float4 color = base_color * tex_color;

    // SDF-based alpha for rounded rects
    if (input.half_size.x > 0.0 && input.half_size.y > 0.0) {
        float2 local = (input.uv * 2.0 - 1.0) * input.half_size;
        float d = sdf_rounded_rect_4(local, input.half_size, input.corner_radii);

        float soft = abs(input.softness);
        float aa = max(fwidth(d) * 0.75, soft);
        float alpha;
        if (input.softness < 0.0) {
            alpha = smoothstep(-aa, 0.0, d);
        } else {
            alpha = 1.0 - smoothstep(-aa, aa, d);
        }
        color.a *= alpha;

        // Border rendering
        if (input.border_width > 0.0 && input.border_color.a > 0.0) {
            float2 inner_half = input.half_size - float2(input.border_width, input.border_width);
            float4 inner_radii = max(input.corner_radii - float4(input.border_width, input.border_width,
                                     input.border_width, input.border_width), float4(0, 0, 0, 0));
            float d_inner = sdf_rounded_rect_4(local, inner_half, inner_radii);
            float inner_aa = fwidth(d_inner) * 0.75;
            float inner_alpha = 1.0 - smoothstep(-inner_aa, inner_aa, d_inner);

            float border_mask = alpha * (1.0 - inner_alpha);
            float4 border_col = input.border_color;
            border_col.a *= border_mask;

            float4 fill = color;
            fill.a *= inner_alpha;

            // Pre-multiplied alpha compositing
            color = fill + border_col * (1.0 - fill.a);
            color.a = fill.a + border_col.a * (1.0 - fill.a);
        }
    }

    return color;
}
)hlsl";

static const char* text_hlsl = R"hlsl(
// ultragui text shader -- D3D12 HLSL port of text.vert + text.frag
// Compile: dxc -T vs_6_0 -E VSMain -Fo text_vs.cso text.hlsl
//          dxc -T ps_6_0 -E PSMain -Fo text_ps.cso text.hlsl

cbuffer PushConstants : register(b0) {
    float2 scale;
    float2 translate;
};

Texture2D tex : register(t0);
SamplerState samp_nearest : register(s1);

struct VSInput {
    float2 pos          : POSITION;
    float2 uv           : TEXCOORD0;
    uint   color        : COLOR0;
    uint   color2       : COLOR1;
    uint   corner_radii : BLENDINDICES0;
    float  softness     : BLENDWEIGHT0;
    float2 half_size    : TEXCOORD1;
    float  border_width : BLENDWEIGHT1;
    uint   border_color : COLOR2;
};

struct VSOutput {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float3 srgb_to_linear(float3 c) {
    return lerp(c / 12.92, pow((c + 0.055) / 1.055, 2.4), step(0.04045, c));
}

VSOutput VSMain(VSInput input) {
    VSOutput o;
    o.pos = float4(input.pos * scale + translate, 0.0, 1.0);
    o.uv = input.uv;

    float4 col = float4(
        float(input.color & 0xFFu) / 255.0,
        float((input.color >> 8u) & 0xFFu) / 255.0,
        float((input.color >> 16u) & 0xFFu) / 255.0,
        float((input.color >> 24u) & 0xFFu) / 255.0
    );
    col.rgb = srgb_to_linear(col.rgb);
    o.color = col;
    return o;
}

float4 PSMain(VSOutput input) : SV_Target {
    float alpha = tex.Sample(samp_nearest, input.uv).r;
    return float4(input.color.rgb, input.color.a * alpha);
}
)hlsl";

static const char* video_hlsl = R"hlsl(
// ultragui video shader -- D3D12 HLSL port of video.vert + video.frag
// Compile: dxc -T vs_6_0 -E VSMain -Fo video_vs.cso video.hlsl
//          dxc -T ps_6_0 -E PSMain -Fo video_ps.cso video.hlsl

Texture2D tex_y  : register(t0);
Texture2D tex_cb : register(t1);
Texture2D tex_cr : register(t2);
SamplerState samp_linear : register(s0);

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain(uint vid : SV_VertexID) {
    // Fullscreen triangle from vertex index -- no vertex buffer needed.
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };
    float2 uvs[3] = {
        float2(0.0, 0.0),
        float2(2.0, 0.0),
        float2(0.0, 2.0)
    };

    VSOutput o;
    o.pos = float4(positions[vid], 0.0, 1.0);
    o.uv = uvs[vid];
    return o;
}

float4 PSMain(VSOutput input) : SV_Target {
    float y  = tex_y.Sample(samp_linear, input.uv).r;
    float cb = tex_cb.Sample(samp_linear, input.uv).r;
    float cr = tex_cr.Sample(samp_linear, input.uv).r;

    // BT.601 YCbCr to RGB conversion (from pl_mpeg documentation)
    float4 ycbcr = float4(y, cb, cr, 1.0);
    float r = dot(ycbcr, float4(1.16438,  0.00000,  1.59603, -0.87079));
    float g = dot(ycbcr, float4(1.16438, -0.39176, -0.81297,  0.52959));
    float b = dot(ycbcr, float4(1.16438,  2.01723,  0.00000, -1.08139));

    return float4(saturate(r), saturate(g), saturate(b), 1.0);
}
)hlsl";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// vkd3d-utils exports D3DCompile with ms_abi calling convention,
// but DXVK headers declare WINAPI as nothing (sysv_abi). We must
// load D3DCompile via dlsym and call with the correct ABI.
#include <dlfcn.h>

typedef HRESULT(__attribute__((ms_abi)) *
                PFN_D3DCompile_vkd3d)(const void*, SIZE_T, const char*,
                                      const void*,
                                      void*,  // D3D_SHADER_MACRO*, ID3DInclude*
                                      const char*, const char*, UINT, UINT,
                                      ID3D10Blob**, ID3D10Blob**);

static PFN_D3DCompile_vkd3d s_D3DCompile = nullptr;

static bool load_d3dcompile() {
  if (s_D3DCompile) return true;
  void* lib = dlopen("libvkd3d-utils.so.1", RTLD_LAZY);
  if (!lib) lib = dlopen("libvkd3d-utils.so", RTLD_LAZY);
  if (!lib) {
    std::fprintf(stderr, "ultragui-d3d11: failed to load libvkd3d-utils.so\n");
    return false;
  }
  s_D3DCompile = (PFN_D3DCompile_vkd3d)dlsym(lib, "D3DCompile");
  if (!s_D3DCompile) {
    std::fprintf(stderr,
                 "ultragui-d3d11: D3DCompile not found in libvkd3d-utils\n");
    return false;
  }
  return true;
}

// The blob returned by vkd3d's D3DCompile uses ms_abi COM vtable.
// DXVK's ID3D10Blob macros use sysv_abi. We need ms_abi wrappers.
struct VkD3DBlob;  // opaque
struct VkD3DBlobVtbl {
  // IUnknown
  HRESULT(__attribute__((ms_abi)) * QueryInterface)(VkD3DBlob*, const IID&,
                                                    void**);
  ULONG(__attribute__((ms_abi)) * AddRef)(VkD3DBlob*);
  ULONG(__attribute__((ms_abi)) * Release)(VkD3DBlob*);
  // ID3D10Blob
  void*(__attribute__((ms_abi)) * GetBufferPointer)(VkD3DBlob*);
  SIZE_T(__attribute__((ms_abi)) * GetBufferSize)(VkD3DBlob*);
};
struct VkD3DBlob {
  const VkD3DBlobVtbl* lpVtbl;
};

struct ShaderBlob {
  std::vector<char> data;
};

static bool compile_shader(const char* source, const char* entry,
                           const char* target, std::vector<char>& out) {
  if (!load_d3dcompile()) return false;

  VkD3DBlob* blob = nullptr;
  VkD3DBlob* errors = nullptr;
  HRESULT hr =
      s_D3DCompile(source, std::strlen(source), "shader", nullptr, nullptr,
                   entry, target, 0, 0, reinterpret_cast<ID3D10Blob**>(&blob),
                   reinterpret_cast<ID3D10Blob**>(&errors));
  if (FAILED(hr)) {
    if (errors) {
      std::fprintf(
          stderr, "ultragui-d3d11: shader compile error (%s/%s):\n%s\n", entry,
          target,
          static_cast<const char*>(errors->lpVtbl->GetBufferPointer(errors)));
      errors->lpVtbl->Release(errors);
    } else {
      std::fprintf(stderr, "ultragui-d3d11: shader compile failed: 0x%08lx\n",
                   hr);
    }
    return false;
  }
  if (errors) errors->lpVtbl->Release(errors);

  void* ptr = blob->lpVtbl->GetBufferPointer(blob);
  SIZE_T len = blob->lpVtbl->GetBufferSize(blob);
  out.assign(static_cast<char*>(ptr), static_cast<char*>(ptr) + len);
  blob->lpVtbl->Release(blob);
  return true;
}

// ---------------------------------------------------------------------------
// RHI::Impl
// ---------------------------------------------------------------------------

static RHI::Impl* s_rhi_instance = nullptr;

struct RHI::Impl {
  // Public API forwarding targets
  bool Init(const RHIConfig& config);
  void Shutdown();
  bool BeginFrame(Color clear_color);
  void EndFrame();
  void SetScissor(Rect rect);
  void ResetScissor();
  void DrawTriangles(const Vertex2D* vertices, u32 vertex_count,
                     const u32* indices, u32 index_count,
                     RHITextureHandle texture);
  void DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count,
                         const u32* indices, u32 index_count,
                         RHITextureHandle atlas_texture);
  RHITextureHandle CreateTexture(u32 width, u32 height, RHIFormat format,
                                 const void* pixels,
                                 RHIFilter filter = RHIFilter::kLinear);
  void UpdateTexture(RHITextureHandle handle, const void* pixels);
  void DestroyTexture(RHITextureHandle handle);
  bool AcquireFrame();
  RHITextureHandle CreateRenderTarget(u32 width, u32 height);
  void DestroyRenderTarget(RHITextureHandle handle);
  bool BeginOffscreen(RHITextureHandle target, Color clear_color);
  void EndOffscreen(RHITextureHandle target);
  void ConvertVideoFrame(RHITextureHandle target, RHITextureHandle y,
                         RHITextureHandle cb, RHITextureHandle cr);
  Vec2 display_size() const;
  f32 dpi_scale() const;

  // Internal helpers
  bool create_shaders();
  bool create_input_layout(const void* vs_bytecode, size_t vs_size);
  bool create_blend_state();
  bool create_rasterizer_state();
  bool create_samplers();
  bool create_projection_cb();
  bool create_default_resources();
  void update_projection(f32 width, f32 height);
  ID3D11Buffer* create_dynamic_buffer(u32 size, UINT bind_flags);
  void ensure_buffer(ID3D11Buffer*& buf, u32& capacity, u32 required,
                     UINT bind_flags, u32 stride);
  bool ensure_video_shaders();
  void bind_quad_pipeline();
  void set_full_scissor();
  void set_full_scissor_for(u32 width, u32 height);

  // -----------------------------------------------------------------------
  // State
  // -----------------------------------------------------------------------

  GLFWwindow* window_ = nullptr;
  f32 dpi_scale_ = 1.0f;
  bool vsync_ = true;

  // D3D11 core
  ID3D11Device* device_ = nullptr;
  ID3D11DeviceContext* ctx_ = nullptr;
  IDXGISwapChain* swapchain_ = nullptr;

  // Swapchain state
  ID3D11RenderTargetView* backbuffer_rtv_ = nullptr;
  u32 swapchain_width_ = 0;
  u32 swapchain_height_ = 0;

  // Shaders
  ID3D11VertexShader* quad_vs_ = nullptr;
  ID3D11PixelShader* quad_ps_ = nullptr;
  ID3D11VertexShader* text_vs_ = nullptr;
  ID3D11PixelShader* text_ps_ = nullptr;
  ID3D11VertexShader* video_vs_ = nullptr;
  ID3D11PixelShader* video_ps_ = nullptr;

  // Input layout (shared by quad and text -- same vertex format)
  ID3D11InputLayout* input_layout_ = nullptr;

  // State objects
  ID3D11BlendState* blend_state_ = nullptr;
  ID3D11RasterizerState* raster_state_ = nullptr;
  ID3D11SamplerState* sampler_linear_ = nullptr;
  ID3D11SamplerState* sampler_nearest_ = nullptr;

  // Constant buffer for projection (scale.xy, translate.xy -- 16 bytes)
  ID3D11Buffer* projection_cb_ = nullptr;

  // Dynamic vertex/index buffers (USAGE_DYNAMIC)
  ID3D11Buffer* vertex_buf_ = nullptr;
  u32 vertex_capacity_ = 0;
  u32 vertex_write_pos_ = 0;

  ID3D11Buffer* index_buf_ = nullptr;
  u32 index_capacity_ = 0;
  u32 index_write_pos_ = 0;

  // Separate text buffers
  ID3D11Buffer* text_vertex_buf_ = nullptr;
  u32 text_vertex_capacity_ = 0;
  u32 text_vertex_write_pos_ = 0;

  ID3D11Buffer* text_index_buf_ = nullptr;
  u32 text_index_capacity_ = 0;
  u32 text_index_write_pos_ = 0;

  // Vertex dedup
  const Vertex2D* last_quad_verts_ = nullptr;
  u32 last_quad_vert_count_ = 0;
  u32 last_quad_vb_offset_ = 0;

  const Vertex2D* last_text_verts_ = nullptr;
  u32 last_text_vert_count_ = 0;
  u32 last_text_vb_offset_ = 0;

  // First draw of frame (use MAP_WRITE_DISCARD vs MAP_WRITE_NO_OVERWRITE)
  bool first_quad_draw_ = true;
  bool first_text_draw_ = true;

  // Textures
  static constexpr u32 MAX_TEXTURES = 256;
  struct TextureSlot {
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;  // only for render targets
    u32 width = 0;
    u32 height = 0;
    u32 pixel_size = 0;
    bool in_use = false;
    bool is_render_target = false;
  };
  TextureSlot textures_[MAX_TEXTURES] = {};
  RHITextureHandle white_texture_ = kInvalidTexture;

  // Offscreen
  RHITextureHandle active_offscreen_target_ = kInvalidTexture;
  Vec2 offscreen_display_size_ = {};
  bool frame_active_ = false;

  bool framebuffer_resized_ = false;
};

// ---------------------------------------------------------------------------
// Dynamic buffer helpers
// ---------------------------------------------------------------------------

ID3D11Buffer* RHI::Impl::create_dynamic_buffer(u32 size, UINT bind_flags) {
  D3D11_BUFFER_DESC desc = {};
  desc.ByteWidth = size;
  desc.Usage = D3D11_USAGE_DYNAMIC;
  desc.BindFlags = bind_flags;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  ID3D11Buffer* buf = nullptr;
  HRESULT hr = ID3D11Device_CreateBuffer(device_, &desc, nullptr, &buf);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreateBuffer failed: 0x%08lx\n", hr);
    return nullptr;
  }
  return buf;
}

void RHI::Impl::ensure_buffer(ID3D11Buffer*& buf, u32& capacity, u32 required,
                              UINT bind_flags, u32 stride) {
  if (capacity >= required) return;

  if (buf) {
    ID3D11Buffer_Release(buf);
    buf = nullptr;
  }

  u32 new_cap = std::max(required, capacity * 2);
  new_cap = std::max(new_cap, 16384u);
  buf = create_dynamic_buffer(new_cap * stride, bind_flags);
  capacity = new_cap;
}

// ---------------------------------------------------------------------------
// Projection constant buffer
// ---------------------------------------------------------------------------

void RHI::Impl::update_projection(f32 width, f32 height) {
  // D3D11 NDC: Y-up (bottom=-1, top=+1). Negate Y to flip to screen coords
  // (Y-down).
  float push[4] = {
      2.0f / width,
      -2.0f / height,
      -1.0f,
      1.0f,
  };

  D3D11_MAPPED_SUBRESOURCE mapped = {};
  HRESULT hr = ID3D11DeviceContext_Map(ctx_, (ID3D11Resource*)projection_cb_, 0,
                                       D3D11_MAP_WRITE_DISCARD, 0, &mapped);
  if (SUCCEEDED(hr)) {
    std::memcpy(mapped.pData, push, sizeof(push));
    ID3D11DeviceContext_Unmap(ctx_, (ID3D11Resource*)projection_cb_, 0);
  }

  ID3D11DeviceContext_VSSetConstantBuffers(ctx_, 0, 1, &projection_cb_);
}

// ---------------------------------------------------------------------------
// Pipeline binding helpers
// ---------------------------------------------------------------------------

void RHI::Impl::bind_quad_pipeline() {
  ID3D11DeviceContext_VSSetShader(ctx_, quad_vs_, nullptr, 0);
  ID3D11DeviceContext_PSSetShader(ctx_, quad_ps_, nullptr, 0);
}

void RHI::Impl::set_full_scissor() {
  if (active_offscreen_target_ != kInvalidTexture) {
    auto& slot = textures_[active_offscreen_target_];
    set_full_scissor_for(slot.width, slot.height);
  } else {
    set_full_scissor_for(swapchain_width_, swapchain_height_);
  }
}

void RHI::Impl::set_full_scissor_for(u32 width, u32 height) {
  D3D11_RECT scissor = {};
  scissor.left = 0;
  scissor.top = 0;
  scissor.right = static_cast<LONG>(width);
  scissor.bottom = static_cast<LONG>(height);
  ID3D11DeviceContext_RSSetScissorRects(ctx_, 1, &scissor);
}

// ---------------------------------------------------------------------------
// Shader creation
// ---------------------------------------------------------------------------

bool RHI::Impl::create_shaders() {
  std::vector<char> bytecode;

  // Quad vertex shader
  if (!compile_shader(quad_hlsl, "VSMain", "vs_5_0", bytecode)) return false;
  HRESULT hr = ID3D11Device_CreateVertexShader(
      device_, bytecode.data(), bytecode.size(), nullptr, &quad_vs_);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreateVertexShader (quad) failed\n");
    return false;
  }

  // Create input layout from quad VS bytecode
  if (!create_input_layout(bytecode.data(), bytecode.size())) return false;

  // Quad pixel shader
  if (!compile_shader(quad_hlsl, "PSMain", "ps_5_0", bytecode)) return false;
  hr = ID3D11Device_CreatePixelShader(device_, bytecode.data(), bytecode.size(),
                                      nullptr, &quad_ps_);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreatePixelShader (quad) failed\n");
    return false;
  }

  // Text vertex shader
  if (!compile_shader(text_hlsl, "VSMain", "vs_5_0", bytecode)) return false;
  hr = ID3D11Device_CreateVertexShader(device_, bytecode.data(),
                                       bytecode.size(), nullptr, &text_vs_);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreateVertexShader (text) failed\n");
    return false;
  }

  // Text pixel shader
  if (!compile_shader(text_hlsl, "PSMain", "ps_5_0", bytecode)) return false;
  hr = ID3D11Device_CreatePixelShader(device_, bytecode.data(), bytecode.size(),
                                      nullptr, &text_ps_);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreatePixelShader (text) failed\n");
    return false;
  }

  // Video shaders are lazy-created on first use
  return true;
}

bool RHI::Impl::create_input_layout(const void* vs_bytecode, size_t vs_size) {
  D3D11_INPUT_ELEMENT_DESC layout[] = {
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

  HRESULT hr = ID3D11Device_CreateInputLayout(device_, layout, 9, vs_bytecode,
                                              vs_size, &input_layout_);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreateInputLayout failed: 0x%08lx\n",
                 hr);
    return false;
  }
  return true;
}

bool RHI::Impl::create_blend_state() {
  D3D11_BLEND_DESC bd = {};
  bd.RenderTarget[0].BlendEnable = TRUE;
  bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
  bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

  HRESULT hr = ID3D11Device_CreateBlendState(device_, &bd, &blend_state_);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreateBlendState failed: 0x%08lx\n",
                 hr);
    return false;
  }
  return true;
}

bool RHI::Impl::create_rasterizer_state() {
  D3D11_RASTERIZER_DESC rd = {};
  rd.FillMode = D3D11_FILL_SOLID;
  rd.CullMode = D3D11_CULL_NONE;
  rd.FrontCounterClockwise = TRUE;
  rd.ScissorEnable = TRUE;  // Always TRUE, just change the rect

  HRESULT hr = ID3D11Device_CreateRasterizerState(device_, &rd, &raster_state_);
  if (FAILED(hr)) {
    std::fprintf(stderr,
                 "ultragui-d3d11: CreateRasterizerState failed: 0x%08lx\n", hr);
    return false;
  }
  return true;
}

bool RHI::Impl::create_samplers() {
  // Linear clamp sampler (slot 0)
  {
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr =
        ID3D11Device_CreateSamplerState(device_, &sd, &sampler_linear_);
    if (FAILED(hr)) {
      std::fprintf(
          stderr,
          "ultragui-d3d11: CreateSamplerState (linear) failed: 0x%08lx\n", hr);
      return false;
    }
  }

  // Nearest clamp sampler (slot 1)
  {
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr =
        ID3D11Device_CreateSamplerState(device_, &sd, &sampler_nearest_);
    if (FAILED(hr)) {
      std::fprintf(
          stderr,
          "ultragui-d3d11: CreateSamplerState (nearest) failed: 0x%08lx\n", hr);
      return false;
    }
  }

  return true;
}

bool RHI::Impl::create_projection_cb() {
  // CB must be a multiple of 16 bytes. We need 16 bytes (4 floats).
  D3D11_BUFFER_DESC desc = {};
  desc.ByteWidth = 16;
  desc.Usage = D3D11_USAGE_DYNAMIC;
  desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  HRESULT hr =
      ID3D11Device_CreateBuffer(device_, &desc, nullptr, &projection_cb_);
  if (FAILED(hr)) {
    std::fprintf(
        stderr,
        "ultragui-d3d11: CreateBuffer (projection CB) failed: 0x%08lx\n", hr);
    return false;
  }
  return true;
}

bool RHI::Impl::create_default_resources() {
  // Create 1x1 white fallback texture
  u32 white_pixel = 0xFFFFFFFF;
  white_texture_ = CreateTexture(1, 1, RHIFormat::kRgba8Unorm, &white_pixel);
  if (white_texture_ == kInvalidTexture) {
    std::fprintf(stderr,
                 "ultragui-d3d11: failed to create white fallback texture\n");
    return false;
  }
  return true;
}

bool RHI::Impl::ensure_video_shaders() {
  if (video_vs_ && video_ps_) return true;

  std::vector<char> bytecode;
  if (!compile_shader(video_hlsl, "VSMain", "vs_5_0", bytecode)) return false;
  HRESULT hr = ID3D11Device_CreateVertexShader(
      device_, bytecode.data(), bytecode.size(), nullptr, &video_vs_);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreateVertexShader (video) failed\n");
    return false;
  }

  if (!compile_shader(video_hlsl, "PSMain", "ps_5_0", bytecode)) return false;
  hr = ID3D11Device_CreatePixelShader(device_, bytecode.data(), bytecode.size(),
                                      nullptr, &video_ps_);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreatePixelShader (video) failed\n");
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// Swapchain back buffer management
// ---------------------------------------------------------------------------

static bool create_backbuffer_rtv(ID3D11Device* device,
                                  IDXGISwapChain* swapchain,
                                  ID3D11RenderTargetView** rtv_out) {
  ID3D11Texture2D* backbuffer = nullptr;
  HRESULT hr = IDXGISwapChain_GetBuffer(swapchain, 0, IID_ID3D11Texture2D,
                                        reinterpret_cast<void**>(&backbuffer));
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: GetBuffer failed: 0x%08lx\n", hr);
    return false;
  }

  hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)backbuffer,
                                           nullptr, rtv_out);
  ID3D11Texture2D_Release(backbuffer);
  if (FAILED(hr)) {
    std::fprintf(
        stderr, "ultragui-d3d11: CreateRenderTargetView failed: 0x%08lx\n", hr);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

bool RHI::Impl::Init(const RHIConfig& config) {
  window_ = static_cast<GLFWwindow*>(config.platform->native_handle());
  vsync_ = config.vsync;

  // Compute DPI scale
  {
    int fw, fh, ww, wh;
    glfwGetFramebufferSize(window_, &fw, &fh);
    glfwGetWindowSize(window_, &ww, &wh);
    dpi_scale_ = (ww > 0) ? static_cast<f32>(fw) / static_cast<f32>(ww) : 1.0f;
  }

  // Use a static to avoid glfwSetWindowUserPointer conflict with InputRouter
  s_rhi_instance = this;
  glfwSetFramebufferSizeCallback(window_, [](GLFWwindow*, int, int) {
    if (!s_rhi_instance) return;
    s_rhi_instance->framebuffer_resized_ = true;
    int fw2, fh2, ww2, wh2;
    glfwGetFramebufferSize(s_rhi_instance->window_, &fw2, &fh2);
    glfwGetWindowSize(s_rhi_instance->window_, &ww2, &wh2);
    s_rhi_instance->dpi_scale_ =
        (ww2 > 0) ? static_cast<f32>(fw2) / static_cast<f32>(ww2) : 1.0f;
  });

  // Get framebuffer size for swapchain
  int fb_w, fb_h;
  glfwGetFramebufferSize(window_, &fb_w, &fb_h);
  swapchain_width_ = static_cast<u32>(fb_w);
  swapchain_height_ = static_cast<u32>(fb_h);

  // Set up swap chain description
  DXGI_SWAP_CHAIN_DESC sc_desc = {};
  sc_desc.BufferDesc.Width = swapchain_width_;
  sc_desc.BufferDesc.Height = swapchain_height_;
  sc_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  sc_desc.BufferDesc.RefreshRate.Numerator = 0;
  sc_desc.BufferDesc.RefreshRate.Denominator = 1;
  sc_desc.SampleDesc.Count = 1;
  sc_desc.SampleDesc.Quality = 0;
  sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sc_desc.BufferCount = 2;
  sc_desc.OutputWindow =
      (HWND)window_;  // GLFWwindow* cast for DXVK-native GLFW WSI
  sc_desc.Windowed = TRUE;
  sc_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
  };
  D3D_FEATURE_LEVEL achieved_level = D3D_FEATURE_LEVEL_11_0;

  // Create device first (no swapchain)
  HRESULT hr = D3D11CreateDevice(nullptr,  // adapter (default)
                                 D3D_DRIVER_TYPE_HARDWARE,
                                 nullptr,  // software rasterizer module
                                 0,        // flags
                                 feature_levels, 2, D3D11_SDK_VERSION, &device_,
                                 &achieved_level, &ctx_);

  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: D3D11CreateDevice failed: 0x%08lx\n",
                 hr);
    return false;
  }

  std::fprintf(stderr, "ultragui-d3d11: device created (FL 0x%x)\n",
               achieved_level);

  // Get DXGI factory from device to create swapchain
  IDXGIDevice* dxgi_device = nullptr;
  hr = ID3D11Device_QueryInterface(device_, IID_IDXGIDevice,
                                   (void**)&dxgi_device);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: QI for IDXGIDevice failed: 0x%08lx\n",
                 hr);
    return false;
  }

  IDXGIAdapter* adapter = nullptr;
  hr = IDXGIDevice_GetAdapter(dxgi_device, &adapter);
  IDXGIDevice_Release(dxgi_device);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: GetAdapter failed: 0x%08lx\n", hr);
    return false;
  }

  IDXGIFactory* factory = nullptr;
  hr = IDXGIAdapter_GetParent(adapter, IID_IDXGIFactory, (void**)&factory);
  IDXGIAdapter_Release(adapter);
  if (FAILED(hr)) {
    std::fprintf(stderr,
                 "ultragui-d3d11: GetParent for IDXGIFactory failed: 0x%08lx\n",
                 hr);
    return false;
  }

  std::fprintf(stderr,
               "ultragui-d3d11: got DXGI factory, creating swapchain\n");

  hr = IDXGIFactory_CreateSwapChain(factory, (IUnknown*)device_, &sc_desc,
                                    &swapchain_);
  IDXGIFactory_Release(factory);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreateSwapChain failed: 0x%08lx\n",
                 hr);
    return false;
  }

  std::fprintf(stderr, "ultragui-d3d11: swapchain created\n");

  // Get back buffer RTV
  std::fprintf(stderr, "ultragui-d3d11: creating backbuffer RTV\n");
  if (!create_backbuffer_rtv(device_, swapchain_, &backbuffer_rtv_))
    return false;
  std::fprintf(stderr, "ultragui-d3d11: compiling shaders\n");
  if (!create_shaders()) return false;
  std::fprintf(stderr, "ultragui-d3d11: creating states\n");
  if (!create_blend_state()) return false;
  if (!create_rasterizer_state()) return false;
  if (!create_samplers()) return false;
  if (!create_projection_cb()) return false;
  if (!create_default_resources()) return false;

  std::printf("ultragui-d3d11: initialization complete (%ux%u)\n",
              swapchain_width_, swapchain_height_);
  return true;
}

// ---------------------------------------------------------------------------
// AcquireFrame (no-op for D3D11 -- no explicit sync needed)
// ---------------------------------------------------------------------------

bool RHI::Impl::AcquireFrame() {
  // D3D11 handles synchronization internally. Nothing to do.
  return true;
}

// ---------------------------------------------------------------------------
// BeginFrame
// ---------------------------------------------------------------------------

bool RHI::Impl::BeginFrame(Color clear_color) {
  // Handle resize
  if (framebuffer_resized_) {
    framebuffer_resized_ = false;

    // Release old backbuffer RTV
    if (backbuffer_rtv_) {
      ID3D11RenderTargetView_Release(backbuffer_rtv_);
      backbuffer_rtv_ = nullptr;
    }

    int fb_w, fb_h;
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    if (fb_w == 0 || fb_h == 0) return false;

    swapchain_width_ = static_cast<u32>(fb_w);
    swapchain_height_ = static_cast<u32>(fb_h);

    HRESULT hr =
        IDXGISwapChain_ResizeBuffers(swapchain_, 0, swapchain_width_,
                                     swapchain_height_, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
      std::fprintf(stderr, "ultragui-d3d11: ResizeBuffers failed: 0x%08lx\n",
                   hr);
      return false;
    }

    if (!create_backbuffer_rtv(device_, swapchain_, &backbuffer_rtv_))
      return false;
  }

  // Check for zero-size framebuffer (minimized window)
  if (swapchain_width_ == 0 || swapchain_height_ == 0) return false;

  // Set render target
  ID3D11DeviceContext_OMSetRenderTargets(ctx_, 1, &backbuffer_rtv_, nullptr);

  // Clear
  f32 clear[4] = {clear_color.r, clear_color.g, clear_color.b, clear_color.a};
  ID3D11DeviceContext_ClearRenderTargetView(ctx_, backbuffer_rtv_, clear);

  // Set viewport (framebuffer pixels)
  D3D11_VIEWPORT viewport = {};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = static_cast<f32>(swapchain_width_);
  viewport.Height = static_cast<f32>(swapchain_height_);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  ID3D11DeviceContext_RSSetViewports(ctx_, 1, &viewport);

  // Set scissor to full viewport
  set_full_scissor_for(swapchain_width_, swapchain_height_);

  // Bind quad shaders
  bind_quad_pipeline();

  // Bind input layout
  ID3D11DeviceContext_IASetInputLayout(ctx_, input_layout_);

  // Bind blend state
  f32 blend_factor[4] = {0, 0, 0, 0};
  ID3D11DeviceContext_OMSetBlendState(ctx_, blend_state_, blend_factor,
                                      0xFFFFFFFF);

  // Bind rasterizer state
  ID3D11DeviceContext_RSSetState(ctx_, raster_state_);

  // Bind samplers (slot 0 = linear, slot 1 = nearest)
  ID3D11SamplerState* samplers[2] = {sampler_linear_, sampler_nearest_};
  ID3D11DeviceContext_PSSetSamplers(ctx_, 0, 2, samplers);

  // Update and bind projection CB using WINDOW coordinates (not framebuffer)
  f32 win_w = static_cast<f32>(swapchain_width_) / dpi_scale_;
  f32 win_h = static_cast<f32>(swapchain_height_) / dpi_scale_;
  update_projection(win_w, win_h);

  // Set topology
  ID3D11DeviceContext_IASetPrimitiveTopology(
      ctx_, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Reset buffer write positions and dedup state
  vertex_write_pos_ = 0;
  index_write_pos_ = 0;
  text_vertex_write_pos_ = 0;
  text_index_write_pos_ = 0;

  last_quad_verts_ = nullptr;
  last_quad_vert_count_ = 0;
  last_text_verts_ = nullptr;
  last_text_vert_count_ = 0;

  first_quad_draw_ = true;
  first_text_draw_ = true;

  frame_active_ = true;
  return true;
}

// ---------------------------------------------------------------------------
// EndFrame
// ---------------------------------------------------------------------------

void RHI::Impl::EndFrame() {
  IDXGISwapChain_Present(swapchain_, vsync_ ? 1 : 0, 0);
  frame_active_ = false;
}

// ---------------------------------------------------------------------------
// Scissor
// ---------------------------------------------------------------------------

void RHI::Impl::SetScissor(Rect rect) {
  f32 scale = (active_offscreen_target_ != kInvalidTexture) ? 1.0f : dpi_scale_;

  D3D11_RECT scissor = {};
  scissor.left = static_cast<LONG>(rect.x * scale);
  scissor.top = static_cast<LONG>(rect.y * scale);
  scissor.right = static_cast<LONG>((rect.x + rect.w) * scale);
  scissor.bottom = static_cast<LONG>((rect.y + rect.h) * scale);
  ID3D11DeviceContext_RSSetScissorRects(ctx_, 1, &scissor);
}

void RHI::Impl::ResetScissor() { set_full_scissor(); }

// ---------------------------------------------------------------------------
// DrawTriangles
// ---------------------------------------------------------------------------

void RHI::Impl::DrawTriangles(const Vertex2D* vertices, u32 vertex_count,
                              const u32* indices, u32 index_count,
                              RHITextureHandle texture) {
  if (vertex_count == 0 || index_count == 0) return;

  // Vertex dedup check
  u32 vb_byte_offset;
  if (vertices == last_quad_verts_ && vertex_count == last_quad_vert_count_) {
    vb_byte_offset = last_quad_vb_offset_;
  } else {
    ensure_buffer(vertex_buf_, vertex_capacity_,
                  vertex_write_pos_ + vertex_count, D3D11_BIND_VERTEX_BUFFER,
                  sizeof(Vertex2D));

    vb_byte_offset = vertex_write_pos_ * sizeof(Vertex2D);

    // Map: first draw uses DISCARD to orphan the old buffer,
    // subsequent draws use NO_OVERWRITE to append.
    D3D11_MAP map_type = first_quad_draw_ ? D3D11_MAP_WRITE_DISCARD
                                          : D3D11_MAP_WRITE_NO_OVERWRITE;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = ID3D11DeviceContext_Map(ctx_, (ID3D11Resource*)vertex_buf_, 0,
                                         map_type, 0, &mapped);
    if (SUCCEEDED(hr)) {
      std::memcpy(static_cast<u8*>(mapped.pData) + vb_byte_offset, vertices,
                  vertex_count * sizeof(Vertex2D));
      ID3D11DeviceContext_Unmap(ctx_, (ID3D11Resource*)vertex_buf_, 0);
    }

    last_quad_verts_ = vertices;
    last_quad_vert_count_ = vertex_count;
    last_quad_vb_offset_ = vb_byte_offset;
    vertex_write_pos_ += vertex_count;
    first_quad_draw_ = false;
  }

  // Always append indices
  ensure_buffer(index_buf_, index_capacity_, index_write_pos_ + index_count,
                D3D11_BIND_INDEX_BUFFER, sizeof(u32));

  u32 ib_byte_offset = index_write_pos_ * sizeof(u32);
  {
    D3D11_MAP map_type = (ib_byte_offset == 0) ? D3D11_MAP_WRITE_DISCARD
                                               : D3D11_MAP_WRITE_NO_OVERWRITE;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = ID3D11DeviceContext_Map(ctx_, (ID3D11Resource*)index_buf_, 0,
                                         map_type, 0, &mapped);
    if (SUCCEEDED(hr)) {
      std::memcpy(static_cast<u8*>(mapped.pData) + ib_byte_offset, indices,
                  index_count * sizeof(u32));
      ID3D11DeviceContext_Unmap(ctx_, (ID3D11Resource*)index_buf_, 0);
    }
  }
  index_write_pos_ += index_count;

  // Bind vertex buffer
  UINT stride = sizeof(Vertex2D);
  UINT offset = vb_byte_offset;
  ID3D11DeviceContext_IASetVertexBuffers(ctx_, 0, 1, &vertex_buf_, &stride,
                                         &offset);

  // Bind index buffer
  ID3D11DeviceContext_IASetIndexBuffer(ctx_, index_buf_, DXGI_FORMAT_R32_UINT,
                                       ib_byte_offset);

  // Bind texture SRV to PS slot 0
  RHITextureHandle tex =
      (texture != kInvalidTexture) ? texture : white_texture_;
  if (tex < MAX_TEXTURES && textures_[tex].in_use) {
    ID3D11DeviceContext_PSSetShaderResources(ctx_, 0, 1, &textures_[tex].srv);
  }

  ID3D11DeviceContext_DrawIndexed(ctx_, index_count, 0, 0);
}

// ---------------------------------------------------------------------------
// DrawTextTriangles
// ---------------------------------------------------------------------------

void RHI::Impl::DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count,
                                  const u32* indices, u32 index_count,
                                  RHITextureHandle atlas_texture) {
  if (vertex_count == 0 || index_count == 0) return;

  // Switch to text pipeline
  ID3D11DeviceContext_VSSetShader(ctx_, text_vs_, nullptr, 0);
  ID3D11DeviceContext_PSSetShader(ctx_, text_ps_, nullptr, 0);

  // Re-update projection (display_size may differ for offscreen)
  Vec2 ds = display_size();
  update_projection(ds.x, ds.y);

  // Use SEPARATE buffers for text to avoid overwriting quad data
  u32 vb_byte_offset;
  if (vertices == last_text_verts_ && vertex_count == last_text_vert_count_) {
    vb_byte_offset = last_text_vb_offset_;
  } else {
    ensure_buffer(text_vertex_buf_, text_vertex_capacity_,
                  text_vertex_write_pos_ + vertex_count,
                  D3D11_BIND_VERTEX_BUFFER, sizeof(Vertex2D));

    vb_byte_offset = text_vertex_write_pos_ * sizeof(Vertex2D);

    D3D11_MAP map_type = first_text_draw_ ? D3D11_MAP_WRITE_DISCARD
                                          : D3D11_MAP_WRITE_NO_OVERWRITE;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = ID3D11DeviceContext_Map(
        ctx_, (ID3D11Resource*)text_vertex_buf_, 0, map_type, 0, &mapped);
    if (SUCCEEDED(hr)) {
      std::memcpy(static_cast<u8*>(mapped.pData) + vb_byte_offset, vertices,
                  vertex_count * sizeof(Vertex2D));
      ID3D11DeviceContext_Unmap(ctx_, (ID3D11Resource*)text_vertex_buf_, 0);
    }

    last_text_verts_ = vertices;
    last_text_vert_count_ = vertex_count;
    last_text_vb_offset_ = vb_byte_offset;
    text_vertex_write_pos_ += vertex_count;
    first_text_draw_ = false;
  }

  // Always append text indices
  ensure_buffer(text_index_buf_, text_index_capacity_,
                text_index_write_pos_ + index_count, D3D11_BIND_INDEX_BUFFER,
                sizeof(u32));

  u32 ib_byte_offset = text_index_write_pos_ * sizeof(u32);
  {
    D3D11_MAP map_type = (ib_byte_offset == 0) ? D3D11_MAP_WRITE_DISCARD
                                               : D3D11_MAP_WRITE_NO_OVERWRITE;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = ID3D11DeviceContext_Map(ctx_, (ID3D11Resource*)text_index_buf_,
                                         0, map_type, 0, &mapped);
    if (SUCCEEDED(hr)) {
      std::memcpy(static_cast<u8*>(mapped.pData) + ib_byte_offset, indices,
                  index_count * sizeof(u32));
      ID3D11DeviceContext_Unmap(ctx_, (ID3D11Resource*)text_index_buf_, 0);
    }
  }
  text_index_write_pos_ += index_count;

  // Bind text vertex buffer
  UINT stride = sizeof(Vertex2D);
  UINT vb_offset = vb_byte_offset;
  ID3D11DeviceContext_IASetVertexBuffers(ctx_, 0, 1, &text_vertex_buf_, &stride,
                                         &vb_offset);

  // Bind text index buffer
  ID3D11DeviceContext_IASetIndexBuffer(ctx_, text_index_buf_,
                                       DXGI_FORMAT_R32_UINT, ib_byte_offset);

  // Bind atlas texture SRV to PS slot 0
  if (atlas_texture < MAX_TEXTURES && textures_[atlas_texture].in_use) {
    ID3D11DeviceContext_PSSetShaderResources(ctx_, 0, 1,
                                             &textures_[atlas_texture].srv);
  }

  ID3D11DeviceContext_IASetPrimitiveTopology(
      ctx_, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ID3D11DeviceContext_DrawIndexed(ctx_, index_count, 0, 0);

  // Switch back to quad pipeline and restore projection
  bind_quad_pipeline();
  update_projection(ds.x, ds.y);
}

// ---------------------------------------------------------------------------
// display_size / dpi_scale
// ---------------------------------------------------------------------------

Vec2 RHI::Impl::display_size() const {
  if (active_offscreen_target_ != kInvalidTexture)
    return offscreen_display_size_;
  return {static_cast<f32>(swapchain_width_) / dpi_scale_,
          static_cast<f32>(swapchain_height_) / dpi_scale_};
}

f32 RHI::Impl::dpi_scale() const { return dpi_scale_; }

// ---------------------------------------------------------------------------
// Texture management
// ---------------------------------------------------------------------------

RHITextureHandle RHI::Impl::CreateTexture(u32 width, u32 height,
                                          RHIFormat format, const void* pixels,
                                          RHIFilter /*filter*/) {
  // Find free slot
  RHITextureHandle handle = kInvalidTexture;
  for (u32 i = 0; i < MAX_TEXTURES; ++i) {
    if (!textures_[i].in_use) {
      handle = i;
      break;
    }
  }
  if (handle == kInvalidTexture) return kInvalidTexture;

  auto& slot = textures_[handle];
  DXGI_FORMAT dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
  u32 pixel_size = 4;
  switch (format) {
    case RHIFormat::kRgba8Unorm:
      dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
      pixel_size = 4;
      break;
    case RHIFormat::kBgra8Unorm:
      dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
      pixel_size = 4;
      break;
    case RHIFormat::kR8Unorm:
      dxgi_format = DXGI_FORMAT_R8_UNORM;
      pixel_size = 1;
      break;
    default:
      return kInvalidTexture;
  }

  // Create texture
  D3D11_TEXTURE2D_DESC tex_desc = {};
  tex_desc.Width = width;
  tex_desc.Height = height;
  tex_desc.MipLevels = 1;
  tex_desc.ArraySize = 1;
  tex_desc.Format = dxgi_format;
  tex_desc.SampleDesc.Count = 1;
  tex_desc.SampleDesc.Quality = 0;
  tex_desc.Usage = D3D11_USAGE_DEFAULT;
  tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  tex_desc.CPUAccessFlags = 0;

  D3D11_SUBRESOURCE_DATA init_data = {};
  init_data.pSysMem = pixels;
  init_data.SysMemPitch = width * pixel_size;

  HRESULT hr = ID3D11Device_CreateTexture2D(
      device_, &tex_desc, pixels ? &init_data : nullptr, &slot.texture);
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d11: CreateTexture2D failed: 0x%08lx\n",
                 hr);
    return kInvalidTexture;
  }

  // Create SRV (NULL desc -> auto from texture format)
  hr = ID3D11Device_CreateShaderResourceView(
      device_, (ID3D11Resource*)slot.texture, nullptr, &slot.srv);
  if (FAILED(hr)) {
    std::fprintf(stderr,
                 "ultragui-d3d11: CreateShaderResourceView failed: 0x%08lx\n",
                 hr);
    ID3D11Texture2D_Release(slot.texture);
    slot.texture = nullptr;
    return kInvalidTexture;
  }

  slot.width = width;
  slot.height = height;
  slot.pixel_size = pixel_size;
  slot.in_use = true;
  return handle;
}

void RHI::Impl::UpdateTexture(RHITextureHandle handle, const void* pixels) {
  if (handle >= MAX_TEXTURES || !textures_[handle].in_use) return;

  auto& slot = textures_[handle];
  ID3D11DeviceContext_UpdateSubresource(ctx_, (ID3D11Resource*)slot.texture, 0,
                                        nullptr, pixels,
                                        slot.width * slot.pixel_size, 0);
}

void RHI::Impl::DestroyTexture(RHITextureHandle handle) {
  if (handle >= MAX_TEXTURES || !textures_[handle].in_use) return;
  if (textures_[handle].is_render_target) {
    DestroyRenderTarget(handle);
    return;
  }

  auto& slot = textures_[handle];
  if (slot.srv) ID3D11ShaderResourceView_Release(slot.srv);
  if (slot.texture) ID3D11Texture2D_Release(slot.texture);
  slot = {};
}

// ---------------------------------------------------------------------------
// Render targets
// ---------------------------------------------------------------------------

RHITextureHandle RHI::Impl::CreateRenderTarget(u32 width, u32 height) {
  // Find free slot
  RHITextureHandle handle = kInvalidTexture;
  for (u32 i = 0; i < MAX_TEXTURES; ++i) {
    if (!textures_[i].in_use) {
      handle = i;
      break;
    }
  }
  if (handle == kInvalidTexture) return kInvalidTexture;

  auto& slot = textures_[handle];

  // Create texture with SHADER_RESOURCE | RENDER_TARGET bind flags
  D3D11_TEXTURE2D_DESC tex_desc = {};
  tex_desc.Width = width;
  tex_desc.Height = height;
  tex_desc.MipLevels = 1;
  tex_desc.ArraySize = 1;
  tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  tex_desc.SampleDesc.Count = 1;
  tex_desc.SampleDesc.Quality = 0;
  tex_desc.Usage = D3D11_USAGE_DEFAULT;
  tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  tex_desc.CPUAccessFlags = 0;

  HRESULT hr =
      ID3D11Device_CreateTexture2D(device_, &tex_desc, nullptr, &slot.texture);
  if (FAILED(hr)) {
    std::fprintf(stderr,
                 "ultragui-d3d11: CreateTexture2D (RT) failed: 0x%08lx\n", hr);
    return kInvalidTexture;
  }

  // Create SRV
  hr = ID3D11Device_CreateShaderResourceView(
      device_, (ID3D11Resource*)slot.texture, nullptr, &slot.srv);
  if (FAILED(hr)) {
    std::fprintf(
        stderr,
        "ultragui-d3d11: CreateShaderResourceView (RT) failed: 0x%08lx\n", hr);
    ID3D11Texture2D_Release(slot.texture);
    slot.texture = nullptr;
    return kInvalidTexture;
  }

  // Create RTV
  hr = ID3D11Device_CreateRenderTargetView(
      device_, (ID3D11Resource*)slot.texture, nullptr, &slot.rtv);
  if (FAILED(hr)) {
    std::fprintf(
        stderr, "ultragui-d3d11: CreateRenderTargetView (RT) failed: 0x%08lx\n",
        hr);
    ID3D11ShaderResourceView_Release(slot.srv);
    ID3D11Texture2D_Release(slot.texture);
    slot.srv = nullptr;
    slot.texture = nullptr;
    return kInvalidTexture;
  }

  slot.width = width;
  slot.height = height;
  slot.pixel_size = 4;
  slot.in_use = true;
  slot.is_render_target = true;
  return handle;
}

void RHI::Impl::DestroyRenderTarget(RHITextureHandle handle) {
  if (handle >= MAX_TEXTURES || !textures_[handle].in_use ||
      !textures_[handle].is_render_target)
    return;

  auto& slot = textures_[handle];
  if (slot.rtv) ID3D11RenderTargetView_Release(slot.rtv);
  if (slot.srv) ID3D11ShaderResourceView_Release(slot.srv);
  if (slot.texture) ID3D11Texture2D_Release(slot.texture);
  slot = {};
}

// ---------------------------------------------------------------------------
// Offscreen rendering
// ---------------------------------------------------------------------------

bool RHI::Impl::BeginOffscreen(RHITextureHandle target, Color clear_color) {
  if (target >= MAX_TEXTURES || !textures_[target].in_use ||
      !textures_[target].is_render_target)
    return false;

  auto& slot = textures_[target];

  active_offscreen_target_ = target;
  offscreen_display_size_ = {static_cast<f32>(slot.width),
                             static_cast<f32>(slot.height)};

  // Unbind SRV if this texture is currently bound (D3D11 validation requires
  // it)
  ID3D11ShaderResourceView* null_srv = nullptr;
  ID3D11DeviceContext_PSSetShaderResources(ctx_, 0, 1, &null_srv);

  // Set render target
  ID3D11DeviceContext_OMSetRenderTargets(ctx_, 1, &slot.rtv, nullptr);

  // Clear
  f32 clear[4] = {clear_color.r, clear_color.g, clear_color.b, clear_color.a};
  ID3D11DeviceContext_ClearRenderTargetView(ctx_, slot.rtv, clear);

  // Set viewport
  D3D11_VIEWPORT viewport = {};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = static_cast<f32>(slot.width);
  viewport.Height = static_cast<f32>(slot.height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  ID3D11DeviceContext_RSSetViewports(ctx_, 1, &viewport);

  // Set scissor
  set_full_scissor_for(slot.width, slot.height);

  // Bind quad pipeline
  bind_quad_pipeline();
  ID3D11DeviceContext_IASetInputLayout(ctx_, input_layout_);

  // Bind blend and rasterizer state
  f32 blend_factor[4] = {0, 0, 0, 0};
  ID3D11DeviceContext_OMSetBlendState(ctx_, blend_state_, blend_factor,
                                      0xFFFFFFFF);
  ID3D11DeviceContext_RSSetState(ctx_, raster_state_);

  // Bind samplers
  ID3D11SamplerState* samplers[2] = {sampler_linear_, sampler_nearest_};
  ID3D11DeviceContext_PSSetSamplers(ctx_, 0, 2, samplers);

  // Projection for pixel-exact offscreen coordinates
  update_projection(static_cast<f32>(slot.width),
                    static_cast<f32>(slot.height));

  ID3D11DeviceContext_IASetPrimitiveTopology(
      ctx_, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  return true;
}

void RHI::Impl::EndOffscreen(RHITextureHandle target) {
  if (active_offscreen_target_ == kInvalidTexture) return;

  (void)target;
  active_offscreen_target_ = kInvalidTexture;

  // Restore swapchain render target
  ID3D11DeviceContext_OMSetRenderTargets(ctx_, 1, &backbuffer_rtv_, nullptr);

  // Restore viewport
  D3D11_VIEWPORT viewport = {};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = static_cast<f32>(swapchain_width_);
  viewport.Height = static_cast<f32>(swapchain_height_);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  ID3D11DeviceContext_RSSetViewports(ctx_, 1, &viewport);

  // Restore scissor
  set_full_scissor_for(swapchain_width_, swapchain_height_);

  // Restore pipeline state
  bind_quad_pipeline();
  ID3D11DeviceContext_IASetInputLayout(ctx_, input_layout_);

  f32 blend_factor[4] = {0, 0, 0, 0};
  ID3D11DeviceContext_OMSetBlendState(ctx_, blend_state_, blend_factor,
                                      0xFFFFFFFF);
  ID3D11DeviceContext_RSSetState(ctx_, raster_state_);

  ID3D11SamplerState* samplers[2] = {sampler_linear_, sampler_nearest_};
  ID3D11DeviceContext_PSSetSamplers(ctx_, 0, 2, samplers);

  f32 win_w = static_cast<f32>(swapchain_width_) / dpi_scale_;
  f32 win_h = static_cast<f32>(swapchain_height_) / dpi_scale_;
  update_projection(win_w, win_h);

  ID3D11DeviceContext_IASetPrimitiveTopology(
      ctx_, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ---------------------------------------------------------------------------
// Video frame conversion (YCbCr -> RGBA)
// ---------------------------------------------------------------------------

void RHI::Impl::ConvertVideoFrame(RHITextureHandle target, RHITextureHandle y,
                                  RHITextureHandle cb, RHITextureHandle cr) {
  if (!ensure_video_shaders()) return;
  if (target >= MAX_TEXTURES || !textures_[target].in_use ||
      !textures_[target].is_render_target)
    return;
  if (y >= MAX_TEXTURES || !textures_[y].in_use) return;
  if (cb >= MAX_TEXTURES || !textures_[cb].in_use) return;
  if (cr >= MAX_TEXTURES || !textures_[cr].in_use) return;

  auto& slot = textures_[target];

  // Unbind SRVs that might conflict
  ID3D11ShaderResourceView* null_srvs[3] = {nullptr, nullptr, nullptr};
  ID3D11DeviceContext_PSSetShaderResources(ctx_, 0, 3, null_srvs);

  // Set render target
  ID3D11DeviceContext_OMSetRenderTargets(ctx_, 1, &slot.rtv, nullptr);

  // Clear
  f32 clear[4] = {0, 0, 0, 0};
  ID3D11DeviceContext_ClearRenderTargetView(ctx_, slot.rtv, clear);

  // Set video shaders
  ID3D11DeviceContext_VSSetShader(ctx_, video_vs_, nullptr, 0);
  ID3D11DeviceContext_PSSetShader(ctx_, video_ps_, nullptr, 0);

  // No input layout needed for fullscreen triangle (uses SV_VertexID)
  ID3D11DeviceContext_IASetInputLayout(ctx_, nullptr);

  // Set viewport
  D3D11_VIEWPORT viewport = {};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = static_cast<f32>(slot.width);
  viewport.Height = static_cast<f32>(slot.height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  ID3D11DeviceContext_RSSetViewports(ctx_, 1, &viewport);

  // Set scissor
  set_full_scissor_for(slot.width, slot.height);

  // Bind 3 SRVs (Y=slot0, Cb=slot1, Cr=slot2) to PS slots 0,1,2
  ID3D11ShaderResourceView* srvs[3] = {
      textures_[y].srv,
      textures_[cb].srv,
      textures_[cr].srv,
  };
  ID3D11DeviceContext_PSSetShaderResources(ctx_, 0, 3, srvs);

  // Bind linear sampler
  ID3D11DeviceContext_PSSetSamplers(ctx_, 0, 1, &sampler_linear_);

  // Bind blend state and rasterizer (needed even for fullscreen pass)
  f32 blend_factor[4] = {0, 0, 0, 0};
  ID3D11DeviceContext_OMSetBlendState(ctx_, blend_state_, blend_factor,
                                      0xFFFFFFFF);
  ID3D11DeviceContext_RSSetState(ctx_, raster_state_);

  ID3D11DeviceContext_IASetPrimitiveTopology(
      ctx_, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Draw fullscreen triangle (3 vertices, no vertex buffer)
  ID3D11DeviceContext_Draw(ctx_, 3, 0);

  // Unbind SRVs from PS to allow the target to be read as texture later
  ID3D11DeviceContext_PSSetShaderResources(ctx_, 0, 3, null_srvs);

  // Restore swapchain render target and quad pipeline
  ID3D11DeviceContext_OMSetRenderTargets(ctx_, 1, &backbuffer_rtv_, nullptr);

  // Restore viewport/scissor for swapchain
  D3D11_VIEWPORT restore_vp = {};
  restore_vp.TopLeftX = 0.0f;
  restore_vp.TopLeftY = 0.0f;
  restore_vp.Width = static_cast<f32>(swapchain_width_);
  restore_vp.Height = static_cast<f32>(swapchain_height_);
  restore_vp.MinDepth = 0.0f;
  restore_vp.MaxDepth = 1.0f;
  ID3D11DeviceContext_RSSetViewports(ctx_, 1, &restore_vp);
  set_full_scissor_for(swapchain_width_, swapchain_height_);

  // Restore quad shaders and input layout
  bind_quad_pipeline();
  ID3D11DeviceContext_IASetInputLayout(ctx_, input_layout_);

  // Restore samplers
  ID3D11SamplerState* samplers[2] = {sampler_linear_, sampler_nearest_};
  ID3D11DeviceContext_PSSetSamplers(ctx_, 0, 2, samplers);
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void RHI::Impl::Shutdown() {
  // Flush any pending GPU work
  if (ctx_) ID3D11DeviceContext_Flush(ctx_);

  // Textures
  for (u32 i = 0; i < MAX_TEXTURES; ++i) {
    if (!textures_[i].in_use) continue;
    if (textures_[i].is_render_target) {
      if (textures_[i].rtv) ID3D11RenderTargetView_Release(textures_[i].rtv);
    }
    if (textures_[i].srv) ID3D11ShaderResourceView_Release(textures_[i].srv);
    if (textures_[i].texture) ID3D11Texture2D_Release(textures_[i].texture);
    textures_[i] = {};
  }
  white_texture_ = kInvalidTexture;

  // Dynamic buffers
  if (vertex_buf_) ID3D11Buffer_Release(vertex_buf_);
  if (index_buf_) ID3D11Buffer_Release(index_buf_);
  if (text_vertex_buf_) ID3D11Buffer_Release(text_vertex_buf_);
  if (text_index_buf_) ID3D11Buffer_Release(text_index_buf_);

  // Projection CB
  if (projection_cb_) ID3D11Buffer_Release(projection_cb_);

  // Samplers
  if (sampler_linear_) ID3D11SamplerState_Release(sampler_linear_);
  if (sampler_nearest_) ID3D11SamplerState_Release(sampler_nearest_);

  // Rasterizer state
  if (raster_state_) ID3D11RasterizerState_Release(raster_state_);

  // Blend state
  if (blend_state_) ID3D11BlendState_Release(blend_state_);

  // Input layout
  if (input_layout_) ID3D11InputLayout_Release(input_layout_);

  // Shaders
  if (video_ps_) ID3D11PixelShader_Release(video_ps_);
  if (video_vs_) ID3D11VertexShader_Release(video_vs_);
  if (text_ps_) ID3D11PixelShader_Release(text_ps_);
  if (text_vs_) ID3D11VertexShader_Release(text_vs_);
  if (quad_ps_) ID3D11PixelShader_Release(quad_ps_);
  if (quad_vs_) ID3D11VertexShader_Release(quad_vs_);

  // Backbuffer RTV
  if (backbuffer_rtv_) ID3D11RenderTargetView_Release(backbuffer_rtv_);

  // D3D11 core (release in reverse order)
  if (ctx_) ID3D11DeviceContext_Release(ctx_);
  if (swapchain_) IDXGISwapChain_Release(swapchain_);
  if (device_) ID3D11Device_Release(device_);

  s_rhi_instance = nullptr;
}

// ---------------------------------------------------------------------------
// RHI forwarding methods (PIMPL)
// ---------------------------------------------------------------------------

RHI::RHI() : impl_(new Impl()) {}
RHI::~RHI() { delete impl_; }

bool RHI::Init(const RHIConfig& config) { return impl_->Init(config); }
void RHI::Shutdown() { impl_->Shutdown(); }
bool RHI::BeginFrame(Color clear_color) {
  return impl_->BeginFrame(clear_color);
}
void RHI::EndFrame() { impl_->EndFrame(); }
void RHI::SetScissor(Rect rect) { impl_->SetScissor(rect); }
void RHI::ResetScissor() { impl_->ResetScissor(); }
void RHI::DrawTriangles(const Vertex2D* vertices, u32 vertex_count,
                        const u32* indices, u32 index_count,
                        RHITextureHandle texture) {
  impl_->DrawTriangles(vertices, vertex_count, indices, index_count, texture);
}
void RHI::DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count,
                            const u32* indices, u32 index_count,
                            RHITextureHandle atlas_texture) {
  impl_->DrawTextTriangles(vertices, vertex_count, indices, index_count,
                           atlas_texture);
}
RHITextureHandle RHI::CreateTexture(u32 width, u32 height, RHIFormat format,
                                    const void* pixels, RHIFilter filter) {
  return impl_->CreateTexture(width, height, format, pixels, filter);
}
void RHI::UpdateTexture(RHITextureHandle handle, const void* pixels) {
  impl_->UpdateTexture(handle, pixels);
}
void RHI::DestroyTexture(RHITextureHandle handle) {
  impl_->DestroyTexture(handle);
}
bool RHI::AcquireFrame() { return impl_->AcquireFrame(); }
RHITextureHandle RHI::CreateRenderTarget(u32 width, u32 height) {
  return impl_->CreateRenderTarget(width, height);
}
void RHI::DestroyRenderTarget(RHITextureHandle handle) {
  impl_->DestroyRenderTarget(handle);
}
bool RHI::BeginOffscreen(RHITextureHandle target, Color clear_color) {
  return impl_->BeginOffscreen(target, clear_color);
}
void RHI::EndOffscreen(RHITextureHandle target) { impl_->EndOffscreen(target); }
void RHI::ConvertVideoFrame(RHITextureHandle target, RHITextureHandle y,
                            RHITextureHandle cb, RHITextureHandle cr) {
  impl_->ConvertVideoFrame(target, y, cb, cr);
}
Vec2 RHI::display_size() const { return impl_->display_size(); }
f32 RHI::dpi_scale() const { return impl_->dpi_scale(); }

#pragma clang diagnostic pop

}  // namespace ugui
