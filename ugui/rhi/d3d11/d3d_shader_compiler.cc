// HLSL -> DXBC through a D3DCompile loaded at run time. See
// d3d_shader_compiler.h.
#include <ugui/rhi/d3d11/d3d_shader_compiler.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define CINTERFACE
#define COBJMACROS
// d3d11.h's C++ helper classes call members that CINTERFACE takes away.
#define D3D11_NO_HELPERS

#include <d3d11.h>
#include <d3dcompiler.h>

#include <stdio.h>
#include <string.h>

// D3D11 consumes DXBC, which dxc no longer emits, so this backend compiles the
// HLSL at startup. Windows keeps that compiler in d3dcompiler_47.dll, Linux in
// vkd3d-utils. Loading it by name avoids an import lib and vkd3d's headers.
//
// The two disagree on x86-64: vkd3d gives its COM surface the Microsoft
// convention, DXVK-native declares WINAPI as nothing. Elsewhere a single C ABI
// covers both.
#if defined(_WIN32)
#include <windows.h>
#define UGUI_D3DCOMPILE_ABI WINAPI
#elif defined(__x86_64__)
#include <dlfcn.h>
#define UGUI_D3DCOMPILE_ABI __attribute__((ms_abi))
#else
#include <dlfcn.h>
#define UGUI_D3DCOMPILE_ABI
#endif

namespace ugui {
namespace d3d {
namespace {

typedef HRESULT(UGUI_D3DCOMPILE_ABI* PFN_D3DCompile_ugui)(
    const void*, SIZE_T, const char*, const void*,
    void*,  // D3D_SHADER_MACRO*, ID3DInclude*
    const char*, const char*, UINT, UINT, ID3D10Blob**, ID3D10Blob**);

static PFN_D3DCompile_ugui s_D3DCompile = nullptr;

static bool load_d3dcompile() {
  if (s_D3DCompile) return true;
#if defined(_WIN32)
  HMODULE lib = LoadLibraryA("d3dcompiler_47.dll");
  if (!lib) lib = LoadLibraryA("d3dcompiler_43.dll");
  if (!lib) {
    fprintf(stderr, "ultragui-d3d11: failed to load d3dcompiler\n");
    return false;
  }
  s_D3DCompile = reinterpret_cast<PFN_D3DCompile_ugui>(
      reinterpret_cast<void*>(GetProcAddress(lib, "D3DCompile")));
#else
  void* lib = dlopen("libvkd3d-utils.so.1", RTLD_LAZY);
  if (!lib) lib = dlopen("libvkd3d-utils.so", RTLD_LAZY);
  if (!lib) {
    fprintf(stderr, "ultragui-d3d11: failed to load libvkd3d-utils.so\n");
    return false;
  }
  s_D3DCompile =
      reinterpret_cast<PFN_D3DCompile_ugui>(dlsym(lib, "D3DCompile"));
#endif
  if (!s_D3DCompile) {
    fprintf(stderr, "ultragui-d3d11: D3DCompile not found\n");
    return false;
  }
  return true;
}

// The blob comes from whichever compiler is in use. On Linux its vtable
// follows vkd3d's convention, which DXVK's ID3D10Blob macros cannot call. On
// Windows the blob and the macros are both the SDK's.
#if defined(_WIN32)
using CompilerBlob = ID3D10Blob;
static void* blob_data(CompilerBlob* b) {
  return ID3D10Blob_GetBufferPointer(b);
}
static SIZE_T blob_size(CompilerBlob* b) { return ID3D10Blob_GetBufferSize(b); }
static void blob_release(CompilerBlob* b) { ID3D10Blob_Release(b); }
#else
struct VkD3DBlob;  // opaque
struct VkD3DBlobVtbl {
  // IUnknown
  HRESULT(UGUI_D3DCOMPILE_ABI* QueryInterface)(VkD3DBlob*, const IID&, void**);
  ULONG(UGUI_D3DCOMPILE_ABI* AddRef)(VkD3DBlob*);
  ULONG(UGUI_D3DCOMPILE_ABI* Release)(VkD3DBlob*);
  // ID3D10Blob
  void*(UGUI_D3DCOMPILE_ABI* GetBufferPointer)(VkD3DBlob*);
  SIZE_T(UGUI_D3DCOMPILE_ABI* GetBufferSize)(VkD3DBlob*);
};
struct VkD3DBlob {
  const VkD3DBlobVtbl* lpVtbl;
};
using CompilerBlob = VkD3DBlob;
static void* blob_data(CompilerBlob* b) {
  return b->lpVtbl->GetBufferPointer(b);
}
static SIZE_T blob_size(CompilerBlob* b) { return b->lpVtbl->GetBufferSize(b); }
static void blob_release(CompilerBlob* b) { b->lpVtbl->Release(b); }
#endif

}  // namespace

bool CompileHlsl(const char* source, const char* entry, const char* target,
                 Vector<char>& out, const char* const* defines) {
  if (!load_d3dcompile()) return false;

  constexpr int kMaxDefines = 8;
  D3D_SHADER_MACRO macros[kMaxDefines + 1] = {};
  int count = 0;
  for (; defines && defines[count] && count < kMaxDefines; ++count) {
    macros[count].Name = defines[count];
    macros[count].Definition = "1";
  }

  CompilerBlob* blob = nullptr;
  CompilerBlob* errors = nullptr;
  HRESULT hr =
      s_D3DCompile(source, strlen(source), "shader",
                   count > 0 ? macros : nullptr, nullptr, entry, target, 0, 0,
                   reinterpret_cast<ID3D10Blob**>(&blob),
                   reinterpret_cast<ID3D10Blob**>(&errors));
  if (FAILED(hr)) {
    if (errors) {
      fprintf(stderr,
                   "ultragui-d3d11: shader compile error (%s/%s):\n%s\n", entry,
                   target, static_cast<const char*>(blob_data(errors)));
      blob_release(errors);
    } else {
      fprintf(stderr, "ultragui-d3d11: shader compile failed: 0x%08lx\n",
                   static_cast<unsigned long>(hr));
    }
    return false;
  }
  if (errors) blob_release(errors);

  const char* ptr = static_cast<const char*>(blob_data(blob));
  out.assign(ptr, ptr + blob_size(blob));
  blob_release(blob);
  return true;
}

}  // namespace d3d
}  // namespace ugui
