/* Calls into vkd3d-utils' D3DCompile and its ID3D10Blob from aki on Linux.
 *
 * ugui/rhi/d3d11/d3d_shader_compiler.cc: "The two disagree on x86-64: vkd3d
 * gives its COM surface the Microsoft convention, DXVK-native declares WINAPI
 * as nothing. Elsewhere a single C ABI covers both." aki's extern functions use
 * the C convention of the target, and has no way to name ms_abi, so the calls
 * that cross into vkd3d go through these four functions, which clang compiles
 * with the right convention. On every other architecture the attribute is
 * empty and they are plain calls. */
#include <stddef.h>

#if defined(__x86_64__)
#define UGUI_D3DCOMPILE_ABI __attribute__((ms_abi))
#else
#define UGUI_D3DCOMPILE_ABI
#endif

typedef struct UguiBlob UguiBlob;
typedef struct UguiBlobVtbl {
  int(UGUI_D3DCOMPILE_ABI* QueryInterface)(UguiBlob*, const void*, void**);
  unsigned(UGUI_D3DCOMPILE_ABI* AddRef)(UguiBlob*);
  unsigned(UGUI_D3DCOMPILE_ABI* Release)(UguiBlob*);
  void*(UGUI_D3DCOMPILE_ABI* GetBufferPointer)(UguiBlob*);
  size_t(UGUI_D3DCOMPILE_ABI* GetBufferSize)(UguiBlob*);
} UguiBlobVtbl;
struct UguiBlob {
  const UguiBlobVtbl* lpVtbl;
};

typedef int(UGUI_D3DCOMPILE_ABI* UguiPfnD3DCompile)(const void*, size_t, const char*, const void*, void*, const char*,
                                                    const char*, unsigned, unsigned, void**, void**);

int ugui_d3dcompile(void* fn, const void* source, size_t source_len, const char* name, const void* macros,
                    void* include, const char* entry, const char* target, unsigned flags1, unsigned flags2,
                    void** blob, void** errors) {
  return ((UguiPfnD3DCompile)fn)(source, source_len, name, macros, include, entry, target, flags1, flags2, blob, errors);
}

void* ugui_blob_data(void* blob) { return ((UguiBlob*)blob)->lpVtbl->GetBufferPointer((UguiBlob*)blob); }
size_t ugui_blob_size(void* blob) { return ((UguiBlob*)blob)->lpVtbl->GetBufferSize((UguiBlob*)blob); }
void ugui_blob_release(void* blob) { ((UguiBlob*)blob)->lpVtbl->Release((UguiBlob*)blob); }
