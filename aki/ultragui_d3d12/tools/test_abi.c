/* Checks the bridge against real vkd3d vtables, including stack arguments,
 * 64-bit results, descriptor handles by value and explicit struct results.
 * Run natively or cross-compile with x86_64-linux-gnu-gcc -static and run with
 * qemu-x86_64. Include flags: pkg-config --cflags libvkd3d vulkan.
 * Link with csrc/d3d12_abi.c; no GPU or vkd3d library is needed. */
#define CINTERFACE
#define NOMINMAX
#include <vkd3d.h>
#include <vkd3d_utils.h>
#include <assert.h>
#include <stdio.h>

extern unsigned int ugui_d3d12_release(void *);
extern int ugui_d3d12_dev_create_committed_resource(void *, const void *, unsigned int,
    const void *, unsigned int, const void *, const void *, void *);
extern void ugui_d3d12_cl_set_graphics_root_descriptor_table(void *, unsigned int, D3D12_GPU_DESCRIPTOR_HANDLE);
extern void ugui_d3d12_cl_clear_render_target_view(void *, D3D12_CPU_DESCRIPTOR_HANDLE, const void *, unsigned int, const void *);
extern unsigned long long ugui_d3d12_fence_get_completed_value(void *);
extern void *ugui_d3d12_res_get_desc(void *, void *);
extern int ugui_d3d12_serialize_root_signature(const void *, unsigned int, void *, void *);

static unsigned calls;
static ID3D12Device device;
static ID3D12GraphicsCommandList list;
static ID3D12Fence fence;
static ID3D12Resource resource;
static const D3D12_HEAP_PROPERTIES heap = { .Type = D3D12_HEAP_TYPE_DEFAULT };
static const D3D12_RESOURCE_DESC desc = { .Width = 12345 };
static const D3D12_CLEAR_VALUE clear = { .Format = DXGI_FORMAT_R8_UNORM };
static const GUID iid = { .Data1 = 42 };
static const FLOAT color[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
static const D3D12_RECT rect = { 1, 2, 3, 4 };

static ULONG STDMETHODCALLTYPE release_resource(ID3D12Resource *self) {
  assert(self == &resource);
  ++calls;
  return 17;
}
static HRESULT STDMETHODCALLTYPE create_resource(ID3D12Device *self, const D3D12_HEAP_PROPERTIES *h,
    D3D12_HEAP_FLAGS flags, const D3D12_RESOURCE_DESC *d, D3D12_RESOURCE_STATES state,
    const D3D12_CLEAR_VALUE *c, REFIID id, void **out) {
  assert(self == &device && h == &heap && flags == D3D12_HEAP_FLAG_NONE && d == &desc);
  assert(state == D3D12_RESOURCE_STATE_COPY_DEST && c == &clear && id == &iid);
  *out = &resource;
  ++calls;
  return 31;
}
static void STDMETHODCALLTYPE set_table(ID3D12GraphicsCommandList *self, UINT index, D3D12_GPU_DESCRIPTOR_HANDLE handle) {
  assert(self == &list && index == 7 && handle.ptr == 0x123456789abcdef0ull);
  ++calls;
}
static void STDMETHODCALLTYPE clear_target(ID3D12GraphicsCommandList *self, D3D12_CPU_DESCRIPTOR_HANDLE handle,
    const FLOAT *values, UINT count, const D3D12_RECT *rects) {
  assert(self == &list && handle.ptr == 0xabcdef0123456789ull);
  assert(values == color && count == 1 && rects == &rect);
  ++calls;
}
static UINT64 STDMETHODCALLTYPE completed_value(ID3D12Fence *self) {
  assert(self == &fence);
  ++calls;
  return 0x876543210abcdef0ull;
}
static D3D12_RESOURCE_DESC *STDMETHODCALLTYPE get_desc(ID3D12Resource *self, D3D12_RESOURCE_DESC *out) {
  assert(self == &resource);
  *out = desc;
  ++calls;
  return out;
}

/* The real exported function also uses WINAPI. Mock it so this test runs
 * without a target-architecture vkd3d library. */
HRESULT WINAPI D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC *d, D3D_ROOT_SIGNATURE_VERSION version,
    ID3DBlob **blob, ID3DBlob **errors) {
  assert(d->NumParameters == 12 && version == D3D_ROOT_SIGNATURE_VERSION_1);
  *blob = (ID3DBlob *)&resource;
  *errors = NULL;
  ++calls;
  return 47;
}

int main(void) {
  static const ID3D12DeviceVtbl device_vtbl = { .CreateCommittedResource = create_resource };
  static const ID3D12GraphicsCommandListVtbl list_vtbl = {
    .SetGraphicsRootDescriptorTable = set_table, .ClearRenderTargetView = clear_target };
  static const ID3D12FenceVtbl fence_vtbl = { .GetCompletedValue = completed_value };
  static const ID3D12ResourceVtbl resource_vtbl = { .Release = release_resource, .GetDesc = get_desc };
  device.lpVtbl = &device_vtbl;
  list.lpVtbl = &list_vtbl;
  fence.lpVtbl = &fence_vtbl;
  resource.lpVtbl = &resource_vtbl;
  void *out = NULL;
  assert(ugui_d3d12_dev_create_committed_resource(&device, &heap, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_COPY_DEST, &clear, &iid, &out) == 31 && out == &resource);
  ugui_d3d12_cl_set_graphics_root_descriptor_table(&list, 7, (D3D12_GPU_DESCRIPTOR_HANDLE){0x123456789abcdef0ull});
  ugui_d3d12_cl_clear_render_target_view(&list, (D3D12_CPU_DESCRIPTOR_HANDLE){0xabcdef0123456789ull}, color, 1, &rect);
  assert(ugui_d3d12_fence_get_completed_value(&fence) == 0x876543210abcdef0ull);
  D3D12_RESOURCE_DESC result = {0};
  assert(ugui_d3d12_res_get_desc(&resource, &result) == &result && result.Width == desc.Width);
  assert(ugui_d3d12_release(&resource) == 17);
  D3D12_ROOT_SIGNATURE_DESC root = { .NumParameters = 12 };
  ID3DBlob *blob = NULL, *errors = NULL;
  assert(ugui_d3d12_serialize_root_signature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors) == 47);
  assert(blob == (ID3DBlob *)&resource && errors == NULL && calls == 7);
  puts("7 ABI bridge cases passed");
  return 0;
}
