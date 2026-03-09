#include <ultragui/platform/platform.h>
#include <ultragui/rhi/rhi.h>

// Prevent vkd3d_windows.h min/max macros from breaking C++ headers
#define NOMINMAX
// Define GUIDs inline (otherwise IID_ID3D12Device etc are external references)
#define INITGUID
// Use C-style COM interface (vkd3d is a C library; C++ vtable ABI can mismatch)
#define CINTERFACE
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS

// vkd3d provides D3D12 + Win32 type stubs on Linux
#include <vkd3d.h>
#include <vkd3d_utils.h>

// Undefine min/max if vkd3d_windows.h defined them despite NOMINMAX
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// Vulkan for swapchain presentation (no DXGI on Linux)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

namespace ugui {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Vector<char> read_file(const String& path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) return {};
  auto size = static_cast<size_t>(file.tellg());
  Vector<char> buf(size);
  file.seekg(0);
  file.read(buf.data(), static_cast<std::streamsize>(size));
  return buf;
}

static VkSurfaceFormatKHR choose_surface_format(
    const Vector<VkSurfaceFormatKHR>& formats) {
  for (auto& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return f;
  }
  for (auto& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return f;
  }
  return formats[0];
}

static VkPresentModeKHR choose_present_mode(
    const Vector<VkPresentModeKHR>& modes, bool vsync) {
  if (vsync) return VK_PRESENT_MODE_FIFO_KHR;
  for (auto m : modes) {
    if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& caps,
                                GLFWwindow* window) {
  if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  VkExtent2D ext = {static_cast<u32>(w), static_cast<u32>(h)};
  ext.width = std::clamp(ext.width, caps.minImageExtent.width,
                         caps.maxImageExtent.width);
  ext.height = std::clamp(ext.height, caps.minImageExtent.height,
                          caps.maxImageExtent.height);
  return ext;
}

/// Map VkFormat (sRGB swapchain) to the matching DXGI format.
static DXGI_FORMAT vk_format_to_dxgi(VkFormat vk_fmt) {
  switch (vk_fmt) {
    case VK_FORMAT_B8G8R8A8_SRGB:
      return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB:
      return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    default:
      return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  }
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
  bool create_vkd3d_instance();
  bool create_vk_surface();
  bool pick_physical_device();
  bool create_d3d12_device();
  bool create_vk_swapchain();
  void cleanup_swapchain();
  bool recreate_swapchain();
  bool create_command_resources();
  bool create_descriptor_heaps();
  bool create_root_signature();
  bool create_pipelines();
  bool create_default_resources();
  bool create_upload_buffers();
  bool ensure_video_pipeline();

  Vector<char> load_shader_cso(const char* filename);
  ID3D12Resource* create_upload_buffer(u32 size, void** mapped);
  void ensure_buffer_capacity(ID3D12Resource*& buf, void*& mapped,
                              u32& capacity, u32 required, u32 stride);
  void wait_for_gpu();
  void upload_texture_data(ID3D12Resource* dest, u32 width, u32 height,
                           u32 pixel_size, const void* pixels,
                           D3D12_RESOURCE_STATES before_state);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv_cpu_handle(u32 index);
  D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu_handle(u32 index);
  D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu_handle(u32 index);

  void set_projection_constants(f32 width, f32 height);

  // -----------------------------------------------------------------------
  // State
  // -----------------------------------------------------------------------

  GLFWwindow* window_ = nullptr;
  String shader_dir_;
  f32 dpi_scale_ = 1.0f;

  // vkd3d
  struct vkd3d_instance* vkd3d_inst_ = nullptr;
  ID3D12Device* device_ = nullptr;

  // Vulkan presentation (no DXGI on Linux)
  VkInstance vk_instance_ = VK_NULL_HANDLE;
  VkSurfaceKHR vk_surface_ = VK_NULL_HANDLE;
  VkSwapchainKHR vk_swapchain_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VkPhysicalDevice vk_physical_ = VK_NULL_HANDLE;
  VkQueue vk_present_queue_ = VK_NULL_HANDLE;
  u32 vk_graphics_family_ = 0;
  u32 vk_present_family_ = 0;
  VkFence vk_acquire_fence_ = VK_NULL_HANDLE;
  static constexpr u32 MAX_SWAPCHAIN = 8;
  VkImage vk_swapchain_images_[MAX_SWAPCHAIN] = {};
  ID3D12Resource* swapchain_resources_[MAX_SWAPCHAIN] = {};
  u32 swapchain_count_ = 0;
  u32 image_index_ = 0;
  VkFormat swapchain_vk_format_ = VK_FORMAT_UNDEFINED;
  DXGI_FORMAT swapchain_dxgi_format_ = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  VkExtent2D swapchain_extent_ = {};

  // D3D12 core
  ID3D12CommandQueue* cmd_queue_ = nullptr;
  static constexpr u32 MAX_FRAMES = 2;
  ID3D12CommandAllocator* cmd_allocs_[MAX_FRAMES] = {};
  ID3D12GraphicsCommandList* cmd_lists_[MAX_FRAMES] = {};
  ID3D12Fence* fence_ = nullptr;
  UINT64 fence_values_[MAX_FRAMES] = {};
  UINT64 fence_next_value_ = 1;
  u32 current_frame_ = 0;

  // Descriptor heaps
  ID3D12DescriptorHeap* rtv_heap_ = nullptr;
  u32 rtv_descriptor_size_ = 0;
  u32 rtv_next_index_ = 0;  // next free RTV slot after swapchain RTVs
  ID3D12DescriptorHeap* srv_heap_ = nullptr;
  u32 srv_descriptor_size_ = 0;

  // Pipelines
  ID3D12RootSignature* root_sig_ = nullptr;
  ID3D12PipelineState* quad_pso_ = nullptr;
  ID3D12PipelineState* text_pso_ = nullptr;
  ID3D12PipelineState* video_pso_ = nullptr;
  ID3D12RootSignature* video_root_sig_ = nullptr;

  // Per-frame upload buffers (ring buffer pattern)
  struct FrameBuffers {
    ID3D12Resource* vertex_buf = nullptr;
    void* vertex_mapped = nullptr;
    u32 vertex_capacity = 0;
    u32 vertex_write_pos = 0;

    ID3D12Resource* index_buf = nullptr;
    void* index_mapped = nullptr;
    u32 index_capacity = 0;
    u32 index_write_pos = 0;

    // Separate text buffers (same pattern as Vulkan backend)
    ID3D12Resource* text_vertex_buf = nullptr;
    void* text_vertex_mapped = nullptr;
    u32 text_vertex_capacity = 0;
    u32 text_vertex_write_pos = 0;

    ID3D12Resource* text_index_buf = nullptr;
    void* text_index_mapped = nullptr;
    u32 text_index_capacity = 0;
    u32 text_index_write_pos = 0;
  };
  FrameBuffers frame_bufs_[MAX_FRAMES] = {};

  // Vertex dedup (same pattern as Vulkan)
  const Vertex2D* last_quad_verts_ = nullptr;
  u32 last_quad_vert_count_ = 0;
  u32 last_quad_vb_offset_ = 0;
  const Vertex2D* last_text_verts_ = nullptr;
  u32 last_text_vert_count_ = 0;
  u32 last_text_vb_offset_ = 0;

  // Textures
  static constexpr u32 MAX_TEXTURES = 256;
  struct TextureSlot {
    ID3D12Resource* resource = nullptr;
    u32 width = 0;
    u32 height = 0;
    u32 pixel_size = 0;
    bool in_use = false;
    bool is_render_target = false;
    u32 srv_index = 0;
    u32 rtv_index = 0;  // only for render targets
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  };
  TextureSlot textures_[MAX_TEXTURES] = {};
  RHITextureHandle white_texture_ = kInvalidTexture;

  // Offscreen state
  bool frame_acquired_ = false;
  RHITextureHandle active_offscreen_target_ = kInvalidTexture;
  Vec2 offscreen_display_size_ = {};

  bool framebuffer_resized_ = false;
  bool vsync_ = true;

  // One-shot command resources for texture uploads
  ID3D12CommandAllocator* upload_cmd_alloc_ = nullptr;
  ID3D12GraphicsCommandList* upload_cmd_list_ = nullptr;
};

// ---------------------------------------------------------------------------
// Descriptor handle helpers
// ---------------------------------------------------------------------------

D3D12_CPU_DESCRIPTOR_HANDLE RHI::Impl::rtv_cpu_handle(u32 index) {
  D3D12_CPU_DESCRIPTOR_HANDLE handle =
      ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(rtv_heap_);
  handle.ptr += static_cast<SIZE_T>(index) * rtv_descriptor_size_;
  return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE RHI::Impl::srv_cpu_handle(u32 index) {
  D3D12_CPU_DESCRIPTOR_HANDLE handle =
      ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(srv_heap_);
  handle.ptr += static_cast<SIZE_T>(index) * srv_descriptor_size_;
  return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE RHI::Impl::srv_gpu_handle(u32 index) {
  D3D12_GPU_DESCRIPTOR_HANDLE handle =
      ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(srv_heap_);
  handle.ptr += static_cast<UINT64>(index) * srv_descriptor_size_;
  return handle;
}

// ---------------------------------------------------------------------------
// Shader loading
// ---------------------------------------------------------------------------

Vector<char> RHI::Impl::load_shader_cso(const char* filename) {
  String path = shader_dir_ + "/" + filename;
  auto data = read_file(path);
  if (data.empty()) {
    std::fprintf(stderr, "ultragui-d3d12: failed to load shader '%s'\n",
                 path.c_str());
  }
  return data;
}

// ---------------------------------------------------------------------------
// Upload buffer helpers
// ---------------------------------------------------------------------------

ID3D12Resource* RHI::Impl::create_upload_buffer(u32 size, void** mapped) {
  D3D12_HEAP_PROPERTIES heap_props = {};
  heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width = size;
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  ID3D12Resource* buf = nullptr;
  HRESULT hr = ID3D12Device_CreateCommittedResource(
      device_, &heap_props, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &IID_ID3D12Resource,
      reinterpret_cast<void**>(&buf));
  if (FAILED(hr)) {
    std::fprintf(
        stderr,
        "ultragui-d3d12: CreateCommittedResource (upload) failed: 0x%08lx\n",
        hr);
    return nullptr;
  }

  // Persistently map
  D3D12_RANGE read_range = {0, 0};
  hr = ID3D12Resource_Map(buf, 0, &read_range, mapped);
  if (FAILED(hr)) {
    ID3D12Resource_Release(buf);
    return nullptr;
  }

  return buf;
}

void RHI::Impl::ensure_buffer_capacity(ID3D12Resource*& buf, void*& mapped,
                                       u32& capacity, u32 required,
                                       u32 stride) {
  if (capacity >= required) return;

  if (buf) {
    ID3D12Resource_Unmap(buf, 0, nullptr);
    ID3D12Resource_Release(buf);
    buf = nullptr;
    mapped = nullptr;
  }

  u32 new_cap = std::max(required, capacity * 2);
  new_cap = std::max(new_cap, 16384u);
  buf = create_upload_buffer(new_cap * stride, &mapped);
  capacity = new_cap;
}

void RHI::Impl::wait_for_gpu() {
  if (!cmd_queue_ || !fence_) return;

  UINT64 val = fence_next_value_++;
  ID3D12CommandQueue_Signal(cmd_queue_, fence_, val);

  if (ID3D12Fence_GetCompletedValue(fence_) < val) {
    // Spin-wait (no Win32 events on Linux)
    while (ID3D12Fence_GetCompletedValue(fence_) < val) {
      // yield
    }
  }
}

void RHI::Impl::set_projection_constants(f32 width, f32 height) {
  auto* cmd = cmd_lists_[current_frame_];
  // D3D12 NDC: Y-up (bottom=-1, top=+1). Negate Y to flip to screen coords
  // (Y-down).
  f32 push[4] = {
      2.0f / width,
      -2.0f / height,
      -1.0f,
      1.0f,
  };
  ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(cmd, 0, 4, push, 0);
}

// ---------------------------------------------------------------------------
// Texture upload helper
// ---------------------------------------------------------------------------

void RHI::Impl::upload_texture_data(ID3D12Resource* dest, u32 width, u32 height,
                                    u32 pixel_size, const void* pixels,
                                    D3D12_RESOURCE_STATES before_state) {
  // Get the required layout for the copy
  D3D12_RESOURCE_DESC tex_desc = ID3D12Resource_GetDesc(dest);
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
  UINT64 total_bytes = 0;
  ID3D12Device_GetCopyableFootprints(device_, &tex_desc, 0, 1, 0, &footprint,
                                     nullptr, nullptr, &total_bytes);

  // Create staging buffer
  void* staging_mapped = nullptr;
  ID3D12Resource* staging =
      create_upload_buffer(static_cast<u32>(total_bytes), &staging_mapped);
  if (!staging) return;

  // Copy pixel data row by row (row pitch may differ from width * pixel_size)
  u32 src_row_pitch = width * pixel_size;
  u32 dst_row_pitch = footprint.Footprint.RowPitch;
  auto* src = static_cast<const u8*>(pixels);
  auto* dst = static_cast<u8*>(staging_mapped) + footprint.Offset;
  for (u32 row = 0; row < height; ++row) {
    std::memcpy(dst + row * dst_row_pitch, src + row * src_row_pitch,
                src_row_pitch);
  }

  // Record copy commands
  ID3D12CommandAllocator_Reset(upload_cmd_alloc_);
  ID3D12GraphicsCommandList_Reset(upload_cmd_list_, upload_cmd_alloc_, nullptr);

  // Transition to COPY_DEST if needed
  if (before_state != D3D12_RESOURCE_STATE_COPY_DEST) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dest;
    barrier.Transition.StateBefore = before_state;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ID3D12GraphicsCommandList_ResourceBarrier(upload_cmd_list_, 1, &barrier);
  }

  D3D12_TEXTURE_COPY_LOCATION src_loc = {};
  src_loc.pResource = staging;
  src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src_loc.PlacedFootprint = footprint;

  D3D12_TEXTURE_COPY_LOCATION dst_loc = {};
  dst_loc.pResource = dest;
  dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst_loc.SubresourceIndex = 0;

  ID3D12GraphicsCommandList_CopyTextureRegion(upload_cmd_list_, &dst_loc, 0, 0,
                                              0, &src_loc, nullptr);

  // Transition to PIXEL_SHADER_RESOURCE
  {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dest;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ID3D12GraphicsCommandList_ResourceBarrier(upload_cmd_list_, 1, &barrier);
  }

  ID3D12GraphicsCommandList_Close(upload_cmd_list_);
  ID3D12CommandList* lists[] = {(ID3D12CommandList*)upload_cmd_list_};
  ID3D12CommandQueue_ExecuteCommandLists(cmd_queue_, 1, lists);
  wait_for_gpu();

  ID3D12Resource_Unmap(staging, 0, nullptr);
  ID3D12Resource_Release(staging);
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

bool RHI::Impl::Init(const RHIConfig& config) {
  window_ = static_cast<GLFWwindow*>(config.platform->native_handle());
  vsync_ = config.vsync;
#ifdef ULTRAGUI_HLSL_SHADER_DIR
  shader_dir_ = ULTRAGUI_HLSL_SHADER_DIR;
#else
  shader_dir_ = config.shader_dir ? config.shader_dir : "shaders";
#endif

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

  if (!create_vkd3d_instance()) return false;
  if (!create_vk_surface()) return false;
  if (!pick_physical_device()) return false;
  if (!create_d3d12_device()) return false;
  if (!create_descriptor_heaps()) return false;
  if (!create_vk_swapchain()) return false;
  if (!create_command_resources()) return false;
  if (!create_root_signature()) return false;
  if (!create_pipelines()) return false;
  if (!create_default_resources()) return false;
  if (!create_upload_buffers()) return false;

  // Create VkFence for swapchain image acquisition
  {
    VkFenceCreateInfo fci = {};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(vk_device_, &fci, nullptr, &vk_acquire_fence_) !=
        VK_SUCCESS) {
      std::fprintf(stderr, "ultragui-d3d12: vkCreateFence (acquire) failed\n");
      return false;
    }
  }

  return true;
}

// ---------------------------------------------------------------------------
// vkd3d instance + Vulkan surface
// ---------------------------------------------------------------------------

bool RHI::Impl::create_vkd3d_instance() {
  u32 glfw_ext_count = 0;
  const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
  if (!glfw_exts) {
    std::fprintf(stderr,
                 "ultragui-d3d12: glfwGetRequiredInstanceExtensions failed\n");
    return false;
  }

  // Collect Vulkan instance extensions needed by GLFW + swapchain
  Vector<const char*> vk_extensions(glfw_exts, glfw_exts + glfw_ext_count);

  struct vkd3d_instance_create_info inst_ci = {};
  inst_ci.type = VKD3D_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  inst_ci.pfn_signal_event = vkd3d_signal_event;
  inst_ci.wchar_size = sizeof(WCHAR);
  inst_ci.instance_extensions = vk_extensions.data();
  inst_ci.instance_extension_count = static_cast<u32>(vk_extensions.size());

  HRESULT hr = vkd3d_create_instance(&inst_ci, &vkd3d_inst_);
  if (FAILED(hr)) {
    std::fprintf(stderr,
                 "ultragui-d3d12: vkd3d_create_instance failed: 0x%08lx\n", hr);
    return false;
  }

  vk_instance_ = vkd3d_instance_get_vk_instance(vkd3d_inst_);
  if (!vk_instance_) {
    std::fprintf(
        stderr,
        "ultragui-d3d12: vkd3d_instance_get_vk_instance returned null\n");
    return false;
  }

  std::printf("ultragui-d3d12: vkd3d instance created\n");
  return true;
}

bool RHI::Impl::create_vk_surface() {
  if (glfwCreateWindowSurface(vk_instance_, window_, nullptr, &vk_surface_) !=
      VK_SUCCESS) {
    std::fprintf(stderr, "ultragui-d3d12: glfwCreateWindowSurface failed\n");
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Physical device selection
// ---------------------------------------------------------------------------

bool RHI::Impl::pick_physical_device() {
  u32 count = 0;
  vkEnumeratePhysicalDevices(vk_instance_, &count, nullptr);
  if (count == 0) {
    std::fprintf(stderr, "ultragui-d3d12: no Vulkan physical devices found\n");
    return false;
  }

  Vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(vk_instance_, &count, devices.data());

  for (auto dev : devices) {
    u32 qcount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, nullptr);
    Vector<VkQueueFamilyProperties> qfamilies(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, qfamilies.data());

    bool found_graphics = false, found_present = false;
    for (u32 i = 0; i < qcount; ++i) {
      if ((qfamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
          !found_graphics) {
        vk_graphics_family_ = i;
        found_graphics = true;
      }
      VkBool32 present_support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, vk_surface_,
                                           &present_support);
      if (present_support && !found_present) {
        vk_present_family_ = i;
        found_present = true;
      }
      if (found_graphics && found_present) break;
    }

    if (found_graphics && found_present) {
      vk_physical_ = dev;
      VkPhysicalDeviceProperties props;
      vkGetPhysicalDeviceProperties(dev, &props);
      std::printf("ultragui-d3d12: using GPU '%s'\n", props.deviceName);
      return true;
    }
  }

  std::fprintf(stderr, "ultragui-d3d12: no suitable physical device found\n");
  return false;
}

// ---------------------------------------------------------------------------
// D3D12 device via vkd3d
// ---------------------------------------------------------------------------

bool RHI::Impl::create_d3d12_device() {
  struct vkd3d_device_create_info dev_ci = {};
  dev_ci.type = VKD3D_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dev_ci.minimum_feature_level = D3D_FEATURE_LEVEL_11_0;
  dev_ci.instance = vkd3d_inst_;
  dev_ci.instance_create_info = nullptr;
  dev_ci.vk_physical_device = vk_physical_;

  // Device extensions required for swapchain presentation
  static const char* device_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  dev_ci.device_extensions = device_exts;
  dev_ci.device_extension_count = 1;

  HRESULT hr = vkd3d_create_device(&dev_ci, &IID_ID3D12Device,
                                   reinterpret_cast<void**>(&device_));
  if (FAILED(hr)) {
    std::fprintf(stderr,
                 "ultragui-d3d12: vkd3d_create_device failed: 0x%08lx\n", hr);
    return false;
  }

  // Get the Vulkan device that vkd3d created internally
  vk_device_ = vkd3d_get_vk_device(device_);
  if (!vk_device_) {
    std::fprintf(stderr, "ultragui-d3d12: vkd3d_get_vk_device returned null\n");
    return false;
  }

  // Get the present queue from the vkd3d-created VkDevice
  vkGetDeviceQueue(vk_device_, vk_present_family_, 0, &vk_present_queue_);

  std::printf("ultragui-d3d12: D3D12 device created via vkd3d\n");
  return true;
}

// ---------------------------------------------------------------------------
// Vulkan swapchain (presentation layer)
// ---------------------------------------------------------------------------

bool RHI::Impl::create_vk_swapchain() {
  VkSurfaceCapabilitiesKHR caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_, vk_surface_, &caps);

  u32 fmt_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_, vk_surface_, &fmt_count,
                                       nullptr);
  Vector<VkSurfaceFormatKHR> formats(fmt_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_, vk_surface_, &fmt_count,
                                       formats.data());

  u32 pm_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physical_, vk_surface_,
                                            &pm_count, nullptr);
  Vector<VkPresentModeKHR> modes(pm_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physical_, vk_surface_,
                                            &pm_count, modes.data());

  auto surface_format = choose_surface_format(formats);
  auto present_mode = choose_present_mode(modes, vsync_);
  auto extent = choose_extent(caps, window_);

  u32 image_count = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
    image_count = caps.maxImageCount;

  VkSwapchainCreateInfoKHR ci = {};
  ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  ci.surface = vk_surface_;
  ci.minImageCount = image_count;
  ci.imageFormat = surface_format.format;
  ci.imageColorSpace = surface_format.colorSpace;
  ci.imageExtent = extent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  u32 queue_indices[] = {vk_graphics_family_, vk_present_family_};
  if (vk_graphics_family_ != vk_present_family_) {
    ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    ci.queueFamilyIndexCount = 2;
    ci.pQueueFamilyIndices = queue_indices;
  } else {
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  ci.preTransform = caps.currentTransform;
  ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  ci.presentMode = present_mode;
  ci.clipped = VK_TRUE;
  ci.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(vk_device_, &ci, nullptr, &vk_swapchain_) !=
      VK_SUCCESS) {
    std::fprintf(stderr, "ultragui-d3d12: vkCreateSwapchainKHR failed\n");
    return false;
  }

  swapchain_vk_format_ = surface_format.format;
  swapchain_dxgi_format_ = vk_format_to_dxgi(swapchain_vk_format_);
  swapchain_extent_ = extent;

  vkGetSwapchainImagesKHR(vk_device_, vk_swapchain_, &image_count, nullptr);
  swapchain_count_ = std::min(image_count, MAX_SWAPCHAIN);
  vkGetSwapchainImagesKHR(vk_device_, vk_swapchain_, &swapchain_count_,
                          vk_swapchain_images_);

  // Wrap each VkImage as an ID3D12Resource via vkd3d
  for (u32 i = 0; i < swapchain_count_; ++i) {
    D3D12_RESOURCE_DESC res_desc = {};
    res_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    res_desc.Width = swapchain_extent_.width;
    res_desc.Height = swapchain_extent_.height;
    res_desc.DepthOrArraySize = 1;
    res_desc.MipLevels = 1;
    res_desc.Format = swapchain_dxgi_format_;
    res_desc.SampleDesc.Count = 1;
    res_desc.SampleDesc.Quality = 0;
    res_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    res_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    struct vkd3d_image_resource_create_info img_ci = {};
    img_ci.type = VKD3D_STRUCTURE_TYPE_IMAGE_RESOURCE_CREATE_INFO;
    img_ci.vk_image = vk_swapchain_images_[i];
    img_ci.desc = res_desc;
    img_ci.flags = VKD3D_RESOURCE_INITIAL_STATE_TRANSITION |
                   VKD3D_RESOURCE_PRESENT_STATE_TRANSITION;
    img_ci.present_state = D3D12_RESOURCE_STATE_PRESENT;

    HRESULT hr =
        vkd3d_create_image_resource(device_, &img_ci, &swapchain_resources_[i]);
    if (FAILED(hr)) {
      std::fprintf(
          stderr,
          "ultragui-d3d12: vkd3d_create_image_resource[%u] failed: 0x%08lx\n",
          i, hr);
      return false;
    }
  }

  // Create RTVs for swapchain images
  rtv_next_index_ = 0;
  for (u32 i = 0; i < swapchain_count_; ++i) {
    ID3D12Device_CreateRenderTargetView(device_, swapchain_resources_[i],
                                        nullptr, rtv_cpu_handle(i));
  }
  rtv_next_index_ = swapchain_count_;

  // Update DPI scale
  {
    int fw, fh, ww, wh;
    glfwGetFramebufferSize(window_, &fw, &fh);
    glfwGetWindowSize(window_, &ww, &wh);
    dpi_scale_ = (ww > 0) ? static_cast<f32>(fw) / static_cast<f32>(ww) : 1.0f;
  }

  return true;
}

void RHI::Impl::cleanup_swapchain() {
  for (u32 i = 0; i < swapchain_count_; ++i) {
    if (swapchain_resources_[i]) {
      vkd3d_resource_decref(swapchain_resources_[i]);
      swapchain_resources_[i] = nullptr;
    }
    vk_swapchain_images_[i] = VK_NULL_HANDLE;
  }
  swapchain_count_ = 0;

  if (vk_swapchain_) {
    vkDestroySwapchainKHR(vk_device_, vk_swapchain_, nullptr);
    vk_swapchain_ = VK_NULL_HANDLE;
  }
}

bool RHI::Impl::recreate_swapchain() {
  int w = 0, h = 0;
  glfwGetFramebufferSize(window_, &w, &h);
  while (w == 0 || h == 0) {
    glfwGetFramebufferSize(window_, &w, &h);
    glfwWaitEvents();
  }

  // Wait for all GPU work to complete
  wait_for_gpu();

  cleanup_swapchain();
  if (!create_vk_swapchain()) return false;
  return true;
}

// ---------------------------------------------------------------------------
// Command resources
// ---------------------------------------------------------------------------

bool RHI::Impl::create_command_resources() {
  // Create D3D12 command queue
  D3D12_COMMAND_QUEUE_DESC queue_desc = {};
  queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

  HRESULT hr = ID3D12Device_CreateCommandQueue(
      device_, &queue_desc, &IID_ID3D12CommandQueue,
      reinterpret_cast<void**>(&cmd_queue_));
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d12: CreateCommandQueue failed: 0x%08lx\n",
                 hr);
    return false;
  }

  // Create per-frame command allocators and command lists
  for (u32 i = 0; i < MAX_FRAMES; ++i) {
    hr = ID3D12Device_CreateCommandAllocator(
        device_, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
        reinterpret_cast<void**>(&cmd_allocs_[i]));
    if (FAILED(hr)) {
      std::fprintf(stderr,
                   "ultragui-d3d12: CreateCommandAllocator[%u] failed\n", i);
      return false;
    }

    hr = ID3D12Device_CreateCommandList(
        device_, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocs_[i], nullptr,
        &IID_ID3D12GraphicsCommandList,
        reinterpret_cast<void**>(&cmd_lists_[i]));
    if (FAILED(hr)) {
      std::fprintf(stderr, "ultragui-d3d12: CreateCommandList[%u] failed\n", i);
      return false;
    }
    // Command lists are created in the recording state; close them
    ID3D12GraphicsCommandList_Close(cmd_lists_[i]);
  }

  // Create fence
  hr = ID3D12Device_CreateFence(device_, 0, D3D12_FENCE_FLAG_NONE,
                                &IID_ID3D12Fence,
                                reinterpret_cast<void**>(&fence_));
  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d12: CreateFence failed\n");
    return false;
  }

  // Upload command resources (for texture uploads outside frame)
  hr = ID3D12Device_CreateCommandAllocator(
      device_, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
      reinterpret_cast<void**>(&upload_cmd_alloc_));
  if (FAILED(hr)) return false;

  hr = ID3D12Device_CreateCommandList(
      device_, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, upload_cmd_alloc_, nullptr,
      &IID_ID3D12GraphicsCommandList,
      reinterpret_cast<void**>(&upload_cmd_list_));
  if (FAILED(hr)) return false;
  ID3D12GraphicsCommandList_Close(upload_cmd_list_);

  return true;
}

// ---------------------------------------------------------------------------
// Descriptor heaps
// ---------------------------------------------------------------------------

bool RHI::Impl::create_descriptor_heaps() {
  // RTV heap: swapchain images + render target textures
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    desc.NumDescriptors = MAX_SWAPCHAIN + MAX_TEXTURES;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = ID3D12Device_CreateDescriptorHeap(
        device_, &desc, &IID_ID3D12DescriptorHeap,
        reinterpret_cast<void**>(&rtv_heap_));
    if (FAILED(hr)) {
      std::fprintf(stderr,
                   "ultragui-d3d12: CreateDescriptorHeap (RTV) failed\n");
      return false;
    }
    rtv_descriptor_size_ = ID3D12Device_GetDescriptorHandleIncrementSize(
        device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  }

  // SRV heap (shader-visible) for texture descriptors
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = MAX_TEXTURES;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = ID3D12Device_CreateDescriptorHeap(
        device_, &desc, &IID_ID3D12DescriptorHeap,
        reinterpret_cast<void**>(&srv_heap_));
    if (FAILED(hr)) {
      std::fprintf(stderr,
                   "ultragui-d3d12: CreateDescriptorHeap (SRV) failed\n");
      return false;
    }
    srv_descriptor_size_ = ID3D12Device_GetDescriptorHandleIncrementSize(
        device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  }

  return true;
}

// ---------------------------------------------------------------------------
// Root signature
// ---------------------------------------------------------------------------

bool RHI::Impl::create_root_signature() {
  // param[0]: 4 root constants (b0): VS visibility
  D3D12_ROOT_PARAMETER params[2] = {};
  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  params[0].Constants.ShaderRegister = 0;
  params[0].Constants.RegisterSpace = 0;
  params[0].Constants.Num32BitValues = 4;
  params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

  // param[1]: descriptor table with 1 SRV (t0): PS visibility
  D3D12_DESCRIPTOR_RANGE srv_range = {};
  srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srv_range.NumDescriptors = 1;
  srv_range.BaseShaderRegister = 0;
  srv_range.RegisterSpace = 0;
  srv_range.OffsetInDescriptorsFromTableStart =
      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[1].DescriptorTable.NumDescriptorRanges = 1;
  params[1].DescriptorTable.pDescriptorRanges = &srv_range;
  params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // 2 static samplers: s0 = linear clamp, s1 = nearest clamp
  D3D12_STATIC_SAMPLER_DESC samplers[2] = {};

  samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samplers[0].ShaderRegister = 0;
  samplers[0].RegisterSpace = 0;
  samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  samplers[0].MaxLOD = D3D12_FLOAT32_MAX;

  samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samplers[1].ShaderRegister = 1;
  samplers[1].RegisterSpace = 0;
  samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  samplers[1].MaxLOD = D3D12_FLOAT32_MAX;

  D3D12_ROOT_SIGNATURE_DESC root_desc = {};
  root_desc.NumParameters = 2;
  root_desc.pParameters = params;
  root_desc.NumStaticSamplers = 2;
  root_desc.pStaticSamplers = samplers;
  root_desc.Flags =
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ID3DBlob* sig_blob = nullptr;
  ID3DBlob* error_blob = nullptr;
  HRESULT hr = D3D12SerializeRootSignature(
      &root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &error_blob);
  if (FAILED(hr)) {
    if (error_blob) {
      std::fprintf(
          stderr, "ultragui-d3d12: root signature error: %s\n",
          static_cast<const char*>(ID3D10Blob_GetBufferPointer(error_blob)));
      ID3D10Blob_Release(error_blob);
    }
    if (sig_blob) ID3D10Blob_Release(sig_blob);
    return false;
  }

  hr = ID3D12Device_CreateRootSignature(
      device_, 0, ID3D10Blob_GetBufferPointer(sig_blob),
      ID3D10Blob_GetBufferSize(sig_blob), &IID_ID3D12RootSignature,
      reinterpret_cast<void**>(&root_sig_));
  ID3D10Blob_Release(sig_blob);
  if (error_blob) ID3D10Blob_Release(error_blob);

  if (FAILED(hr)) {
    std::fprintf(stderr, "ultragui-d3d12: CreateRootSignature failed\n");
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// Pipeline state objects
// ---------------------------------------------------------------------------

bool RHI::Impl::create_pipelines() {
  // Vertex input layout (matches Vertex2D: 48 bytes)
  D3D12_INPUT_ELEMENT_DESC input_layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32_UINT, 0, 16,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR", 1, DXGI_FORMAT_R32_UINT, 0, 20,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"BLENDINDICES", 0, DXGI_FORMAT_R32_UINT, 0, 24,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"BLENDWEIGHT", 0, DXGI_FORMAT_R32_FLOAT, 0, 28,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 32,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"BLENDWEIGHT", 1, DXGI_FORMAT_R32_FLOAT, 0, 40,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR", 2, DXGI_FORMAT_R32_UINT, 0, 44,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };

  // Blend state (alpha blending)
  D3D12_RENDER_TARGET_BLEND_DESC rt_blend = {};
  rt_blend.BlendEnable = TRUE;
  rt_blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
  rt_blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  rt_blend.BlendOp = D3D12_BLEND_OP_ADD;
  rt_blend.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  rt_blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  rt_blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
  rt_blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  // --- Quad PSO ---
  {
    auto vs_code = load_shader_cso("quad_vs.cso");
    auto ps_code = load_shader_cso("quad_ps.cso");
    if (vs_code.empty() || ps_code.empty()) {
      std::fprintf(stderr, "ultragui-d3d12: failed to load quad shaders\n");
      return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = root_sig_;
    pso_desc.VS.pShaderBytecode = vs_code.data();
    pso_desc.VS.BytecodeLength = vs_code.size();
    pso_desc.PS.pShaderBytecode = ps_code.data();
    pso_desc.PS.BytecodeLength = ps_code.size();

    pso_desc.BlendState.RenderTarget[0] = rt_blend;
    pso_desc.SampleMask = 0xFFFFFFFFu;

    pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.RasterizerState.DepthClipEnable = TRUE;

    pso_desc.InputLayout.pInputElementDescs = input_layout;
    pso_desc.InputLayout.NumElements = 9;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = swapchain_dxgi_format_;
    pso_desc.SampleDesc.Count = 1;

    HRESULT hr = ID3D12Device_CreateGraphicsPipelineState(
        device_, &pso_desc, &IID_ID3D12PipelineState,
        reinterpret_cast<void**>(&quad_pso_));
    if (FAILED(hr)) {
      std::fprintf(stderr,
                   "ultragui-d3d12: CreateGraphicsPipelineState (quad) failed: "
                   "0x%08lx\n",
                   hr);
      return false;
    }
  }

  // --- Text PSO ---
  {
    auto vs_code = load_shader_cso("text_vs.cso");
    auto ps_code = load_shader_cso("text_ps.cso");
    if (vs_code.empty() || ps_code.empty()) {
      std::fprintf(stderr, "ultragui-d3d12: failed to load text shaders\n");
      return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = root_sig_;
    pso_desc.VS.pShaderBytecode = vs_code.data();
    pso_desc.VS.BytecodeLength = vs_code.size();
    pso_desc.PS.pShaderBytecode = ps_code.data();
    pso_desc.PS.BytecodeLength = ps_code.size();

    pso_desc.BlendState.RenderTarget[0] = rt_blend;
    pso_desc.SampleMask = 0xFFFFFFFFu;

    pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.RasterizerState.DepthClipEnable = TRUE;

    pso_desc.InputLayout.pInputElementDescs = input_layout;
    pso_desc.InputLayout.NumElements = 9;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = swapchain_dxgi_format_;
    pso_desc.SampleDesc.Count = 1;

    HRESULT hr = ID3D12Device_CreateGraphicsPipelineState(
        device_, &pso_desc, &IID_ID3D12PipelineState,
        reinterpret_cast<void**>(&text_pso_));
    if (FAILED(hr)) {
      std::fprintf(stderr,
                   "ultragui-d3d12: CreateGraphicsPipelineState (text) failed: "
                   "0x%08lx\n",
                   hr);
      return false;
    }
  }

  return true;
}

// ---------------------------------------------------------------------------
// Default resources
// ---------------------------------------------------------------------------

bool RHI::Impl::create_default_resources() {
  u32 white_pixel = 0xFFFFFFFF;
  white_texture_ = CreateTexture(1, 1, RHIFormat::kRgba8Unorm, &white_pixel);
  return white_texture_ != kInvalidTexture;
}

// ---------------------------------------------------------------------------
// Upload buffers
// ---------------------------------------------------------------------------

bool RHI::Impl::create_upload_buffers() {
  for (u32 i = 0; i < MAX_FRAMES; ++i) {
    auto& fb = frame_bufs_[i];

    // Quad vertex buffer
    fb.vertex_capacity = 16384;
    fb.vertex_buf = create_upload_buffer(fb.vertex_capacity * sizeof(Vertex2D),
                                         &fb.vertex_mapped);
    if (!fb.vertex_buf) return false;

    // Quad index buffer
    fb.index_capacity = 32768;
    fb.index_buf =
        create_upload_buffer(fb.index_capacity * sizeof(u32), &fb.index_mapped);
    if (!fb.index_buf) return false;

    // Text vertex buffer
    fb.text_vertex_capacity = 16384;
    fb.text_vertex_buf = create_upload_buffer(
        fb.text_vertex_capacity * sizeof(Vertex2D), &fb.text_vertex_mapped);
    if (!fb.text_vertex_buf) return false;

    // Text index buffer
    fb.text_index_capacity = 32768;
    fb.text_index_buf = create_upload_buffer(
        fb.text_index_capacity * sizeof(u32), &fb.text_index_mapped);
    if (!fb.text_index_buf) return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Video pipeline (lazy init)
// ---------------------------------------------------------------------------

bool RHI::Impl::ensure_video_pipeline() {
  if (video_pso_) return true;

  // Video root signature: 3 descriptor tables (t0, t1, t2), 1 sampler
  D3D12_DESCRIPTOR_RANGE ranges[3] = {};
  D3D12_ROOT_PARAMETER params[3] = {};
  for (u32 i = 0; i < 3; ++i) {
    ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[i].NumDescriptors = 1;
    ranges[i].BaseShaderRegister = i;
    ranges[i].RegisterSpace = 0;
    ranges[i].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[i].DescriptorTable.NumDescriptorRanges = 1;
    params[i].DescriptorTable.pDescriptorRanges = &ranges[i];
    params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  }

  D3D12_STATIC_SAMPLER_DESC sampler = {};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.ShaderRegister = 0;
  sampler.RegisterSpace = 0;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  sampler.MaxLOD = D3D12_FLOAT32_MAX;

  D3D12_ROOT_SIGNATURE_DESC root_desc = {};
  root_desc.NumParameters = 3;
  root_desc.pParameters = params;
  root_desc.NumStaticSamplers = 1;
  root_desc.pStaticSamplers = &sampler;
  root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

  ID3DBlob* sig_blob = nullptr;
  ID3DBlob* error_blob = nullptr;
  HRESULT hr = D3D12SerializeRootSignature(
      &root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &error_blob);
  if (FAILED(hr)) {
    if (error_blob) {
      std::fprintf(
          stderr, "ultragui-d3d12: video root sig error: %s\n",
          static_cast<const char*>(ID3D10Blob_GetBufferPointer(error_blob)));
      ID3D10Blob_Release(error_blob);
    }
    if (sig_blob) ID3D10Blob_Release(sig_blob);
    return false;
  }

  hr = ID3D12Device_CreateRootSignature(
      device_, 0, ID3D10Blob_GetBufferPointer(sig_blob),
      ID3D10Blob_GetBufferSize(sig_blob), &IID_ID3D12RootSignature,
      reinterpret_cast<void**>(&video_root_sig_));
  ID3D10Blob_Release(sig_blob);
  if (error_blob) ID3D10Blob_Release(error_blob);
  if (FAILED(hr)) return false;

  // Load video shaders
  auto vs_code = load_shader_cso("video_vs.cso");
  auto ps_code = load_shader_cso("video_ps.cso");
  if (vs_code.empty() || ps_code.empty()) return false;

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
  pso_desc.pRootSignature = video_root_sig_;
  pso_desc.VS.pShaderBytecode = vs_code.data();
  pso_desc.VS.BytecodeLength = vs_code.size();
  pso_desc.PS.pShaderBytecode = ps_code.data();
  pso_desc.PS.BytecodeLength = ps_code.size();

  // No blending (opaque video frame)
  pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
      D3D12_COLOR_WRITE_ENABLE_ALL;
  pso_desc.SampleMask = 0xFFFFFFFFu;

  pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso_desc.RasterizerState.DepthClipEnable = TRUE;

  // No vertex input (fullscreen triangle from SV_VertexID)
  pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso_desc.NumRenderTargets = 1;
  pso_desc.RTVFormats[0] = swapchain_dxgi_format_;
  pso_desc.SampleDesc.Count = 1;

  hr = ID3D12Device_CreateGraphicsPipelineState(
      device_, &pso_desc, &IID_ID3D12PipelineState,
      reinterpret_cast<void**>(&video_pso_));
  if (FAILED(hr)) {
    std::fprintf(
        stderr,
        "ultragui-d3d12: CreateGraphicsPipelineState (video) failed: 0x%08lx\n",
        hr);
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// AcquireFrame
// ---------------------------------------------------------------------------

bool RHI::Impl::AcquireFrame() {
  if (frame_acquired_) return true;

  // Wait for previous frame's fence value
  if (ID3D12Fence_GetCompletedValue(fence_) < fence_values_[current_frame_]) {
    while (ID3D12Fence_GetCompletedValue(fence_) <
           fence_values_[current_frame_]) {
      // spin-wait
    }
  }

  // Acquire next swapchain image via Vulkan
  VkResult result =
      vkAcquireNextImageKHR(vk_device_, vk_swapchain_, UINT64_MAX,
                            VK_NULL_HANDLE, vk_acquire_fence_, &image_index_);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_swapchain();
    return false;
  }

  // Wait for the acquire fence
  vkWaitForFences(vk_device_, 1, &vk_acquire_fence_, VK_TRUE, UINT64_MAX);
  vkResetFences(vk_device_, 1, &vk_acquire_fence_);

  // Reset command allocator and command list
  ID3D12CommandAllocator_Reset(cmd_allocs_[current_frame_]);
  ID3D12GraphicsCommandList_Reset(cmd_lists_[current_frame_],
                                  cmd_allocs_[current_frame_], nullptr);

  // Reset write positions for the ring-buffer
  auto& fb = frame_bufs_[current_frame_];
  fb.vertex_write_pos = 0;
  fb.index_write_pos = 0;
  fb.text_vertex_write_pos = 0;
  fb.text_index_write_pos = 0;

  // Reset vertex dedup tracking
  last_quad_verts_ = nullptr;
  last_quad_vert_count_ = 0;
  last_text_verts_ = nullptr;
  last_text_vert_count_ = 0;

  frame_acquired_ = true;
  return true;
}

// ---------------------------------------------------------------------------
// BeginFrame
// ---------------------------------------------------------------------------

bool RHI::Impl::BeginFrame(Color clear_color) {
  if (!AcquireFrame()) return false;

  auto* cmd = cmd_lists_[current_frame_];

  // Transition swapchain: PRESENT -> RENDER_TARGET
  {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = swapchain_resources_[image_index_];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &barrier);
  }

  // Set render target
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_cpu_handle(image_index_);
  ID3D12GraphicsCommandList_OMSetRenderTargets(cmd, 1, &rtv, FALSE, nullptr);

  // Clear
  f32 clear[4] = {clear_color.r, clear_color.g, clear_color.b, clear_color.a};
  ID3D12GraphicsCommandList_ClearRenderTargetView(cmd, rtv, clear, 0, nullptr);

  // Set viewport (framebuffer pixels)
  D3D12_VIEWPORT viewport = {};
  viewport.Width = static_cast<f32>(swapchain_extent_.width);
  viewport.Height = static_cast<f32>(swapchain_extent_.height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  ID3D12GraphicsCommandList_RSSetViewports(cmd, 1, &viewport);

  // Set scissor (full framebuffer)
  D3D12_RECT scissor = {};
  scissor.right = static_cast<LONG>(swapchain_extent_.width);
  scissor.bottom = static_cast<LONG>(swapchain_extent_.height);
  ID3D12GraphicsCommandList_RSSetScissorRects(cmd, 1, &scissor);

  // Set pipeline state and root signature
  ID3D12GraphicsCommandList_SetPipelineState(cmd, quad_pso_);
  ID3D12GraphicsCommandList_SetGraphicsRootSignature(cmd, root_sig_);

  // Set descriptor heap
  ID3D12DescriptorHeap* heaps[] = {srv_heap_};
  ID3D12GraphicsCommandList_SetDescriptorHeaps(cmd, 1, heaps);

  // Set projection constants using WINDOW coordinates (not framebuffer)
  f32 win_w = static_cast<f32>(swapchain_extent_.width) / dpi_scale_;
  f32 win_h = static_cast<f32>(swapchain_extent_.height) / dpi_scale_;
  set_projection_constants(win_w, win_h);

  // Set topology
  ID3D12GraphicsCommandList_IASetPrimitiveTopology(
      cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  return true;
}

// ---------------------------------------------------------------------------
// EndFrame
// ---------------------------------------------------------------------------

void RHI::Impl::EndFrame() {
  auto* cmd = cmd_lists_[current_frame_];

  // Transition swapchain: RENDER_TARGET -> PRESENT
  {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = swapchain_resources_[image_index_];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &barrier);
  }

  // Close command list
  ID3D12GraphicsCommandList_Close(cmd);

  // Execute
  ID3D12CommandList* lists[] = {(ID3D12CommandList*)cmd};
  ID3D12CommandQueue_ExecuteCommandLists(cmd_queue_, 1, lists);

  // Signal fence
  UINT64 val = fence_next_value_++;
  fence_values_[current_frame_] = val;
  ID3D12CommandQueue_Signal(cmd_queue_, fence_, val);

  // Present via Vulkan
  // Acquire the VkQueue from the D3D12 command queue for presentation
  VkQueue present_queue = vkd3d_acquire_vk_queue(cmd_queue_);

  VkPresentInfoKHR pi = {};
  pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  pi.swapchainCount = 1;
  pi.pSwapchains = &vk_swapchain_;
  pi.pImageIndices = &image_index_;

  VkResult result = vkQueuePresentKHR(present_queue, &pi);

  vkd3d_release_vk_queue(cmd_queue_);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      framebuffer_resized_) {
    framebuffer_resized_ = false;
    recreate_swapchain();
  }

  frame_acquired_ = false;
  current_frame_ = (current_frame_ + 1) % MAX_FRAMES;
}

// ---------------------------------------------------------------------------
// Scissor
// ---------------------------------------------------------------------------

void RHI::Impl::SetScissor(Rect rect) {
  auto* cmd = cmd_lists_[current_frame_];
  f32 scale = (active_offscreen_target_ != kInvalidTexture) ? 1.0f : dpi_scale_;

  D3D12_RECT scissor = {};
  scissor.left = static_cast<LONG>(rect.x * scale);
  scissor.top = static_cast<LONG>(rect.y * scale);
  scissor.right = static_cast<LONG>((rect.x + rect.w) * scale);
  scissor.bottom = static_cast<LONG>((rect.y + rect.h) * scale);
  ID3D12GraphicsCommandList_RSSetScissorRects(cmd, 1, &scissor);
}

void RHI::Impl::ResetScissor() {
  auto* cmd = cmd_lists_[current_frame_];
  D3D12_RECT scissor = {};

  if (active_offscreen_target_ != kInvalidTexture) {
    auto& slot = textures_[active_offscreen_target_];
    scissor.right = static_cast<LONG>(slot.width);
    scissor.bottom = static_cast<LONG>(slot.height);
  } else {
    scissor.right = static_cast<LONG>(swapchain_extent_.width);
    scissor.bottom = static_cast<LONG>(swapchain_extent_.height);
  }
  ID3D12GraphicsCommandList_RSSetScissorRects(cmd, 1, &scissor);
}

// ---------------------------------------------------------------------------
// DrawTriangles
// ---------------------------------------------------------------------------

void RHI::Impl::DrawTriangles(const Vertex2D* vertices, u32 vertex_count,
                              const u32* indices, u32 index_count,
                              RHITextureHandle texture) {
  if (vertex_count == 0 || index_count == 0) return;

  auto* cmd = cmd_lists_[current_frame_];
  auto& fb = frame_bufs_[current_frame_];

  // Vertex dedup check
  u32 vb_byte_offset;
  if (vertices == last_quad_verts_ && vertex_count == last_quad_vert_count_) {
    vb_byte_offset = last_quad_vb_offset_;
  } else {
    ensure_buffer_capacity(fb.vertex_buf, fb.vertex_mapped, fb.vertex_capacity,
                           fb.vertex_write_pos + vertex_count,
                           sizeof(Vertex2D));
    vb_byte_offset = fb.vertex_write_pos * sizeof(Vertex2D);
    std::memcpy(static_cast<u8*>(fb.vertex_mapped) + vb_byte_offset, vertices,
                vertex_count * sizeof(Vertex2D));
    last_quad_verts_ = vertices;
    last_quad_vert_count_ = vertex_count;
    last_quad_vb_offset_ = vb_byte_offset;
    fb.vertex_write_pos += vertex_count;
  }

  // Always append indices
  ensure_buffer_capacity(fb.index_buf, fb.index_mapped, fb.index_capacity,
                         fb.index_write_pos + index_count, sizeof(u32));
  u32 ib_byte_offset = fb.index_write_pos * sizeof(u32);
  std::memcpy(static_cast<u8*>(fb.index_mapped) + ib_byte_offset, indices,
              index_count * sizeof(u32));
  fb.index_write_pos += index_count;

  // Bind texture
  RHITextureHandle tex =
      (texture != kInvalidTexture) ? texture : white_texture_;
  if (tex < MAX_TEXTURES && textures_[tex].in_use) {
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(
        cmd, 1, srv_gpu_handle(textures_[tex].srv_index));
  }

  // Set vertex buffer view
  D3D12_VERTEX_BUFFER_VIEW vbv = {};
  vbv.BufferLocation =
      ID3D12Resource_GetGPUVirtualAddress(fb.vertex_buf) + vb_byte_offset;
  vbv.SizeInBytes = vertex_count * sizeof(Vertex2D);
  vbv.StrideInBytes = sizeof(Vertex2D);
  ID3D12GraphicsCommandList_IASetVertexBuffers(cmd, 0, 1, &vbv);

  // Set index buffer view
  D3D12_INDEX_BUFFER_VIEW ibv = {};
  ibv.BufferLocation =
      ID3D12Resource_GetGPUVirtualAddress(fb.index_buf) + ib_byte_offset;
  ibv.SizeInBytes = index_count * sizeof(u32);
  ibv.Format = DXGI_FORMAT_R32_UINT;
  ID3D12GraphicsCommandList_IASetIndexBuffer(cmd, &ibv);

  ID3D12GraphicsCommandList_DrawIndexedInstanced(cmd, index_count, 1, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// DrawTextTriangles
// ---------------------------------------------------------------------------

void RHI::Impl::DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count,
                                  const u32* indices, u32 index_count,
                                  RHITextureHandle atlas_texture) {
  if (vertex_count == 0 || index_count == 0) return;

  auto* cmd = cmd_lists_[current_frame_];
  auto& fb = frame_bufs_[current_frame_];

  // Use SEPARATE buffers for text to avoid overwriting quad data
  u32 vb_byte_offset;
  if (vertices == last_text_verts_ && vertex_count == last_text_vert_count_) {
    vb_byte_offset = last_text_vb_offset_;
  } else {
    ensure_buffer_capacity(
        fb.text_vertex_buf, fb.text_vertex_mapped, fb.text_vertex_capacity,
        fb.text_vertex_write_pos + vertex_count, sizeof(Vertex2D));
    vb_byte_offset = fb.text_vertex_write_pos * sizeof(Vertex2D);
    std::memcpy(static_cast<u8*>(fb.text_vertex_mapped) + vb_byte_offset,
                vertices, vertex_count * sizeof(Vertex2D));
    last_text_verts_ = vertices;
    last_text_vert_count_ = vertex_count;
    last_text_vb_offset_ = vb_byte_offset;
    fb.text_vertex_write_pos += vertex_count;
  }

  ensure_buffer_capacity(fb.text_index_buf, fb.text_index_mapped,
                         fb.text_index_capacity,
                         fb.text_index_write_pos + index_count, sizeof(u32));
  u32 ib_byte_offset = fb.text_index_write_pos * sizeof(u32);
  std::memcpy(static_cast<u8*>(fb.text_index_mapped) + ib_byte_offset, indices,
              index_count * sizeof(u32));
  fb.text_index_write_pos += index_count;

  // Switch to text pipeline
  ID3D12GraphicsCommandList_SetPipelineState(cmd, text_pso_);

  // Re-set root signature and descriptor heaps (pipeline change)
  ID3D12GraphicsCommandList_SetGraphicsRootSignature(cmd, root_sig_);
  ID3D12DescriptorHeap* heaps[] = {srv_heap_};
  ID3D12GraphicsCommandList_SetDescriptorHeaps(cmd, 1, heaps);

  // Re-set projection constants (display_size may differ for offscreen)
  Vec2 ds = display_size();
  set_projection_constants(ds.x, ds.y);

  // Bind atlas texture
  RHITextureHandle tex = atlas_texture;
  if (tex < MAX_TEXTURES && textures_[tex].in_use) {
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(
        cmd, 1, srv_gpu_handle(textures_[tex].srv_index));
  }

  // Set vertex and index buffer views
  D3D12_VERTEX_BUFFER_VIEW vbv = {};
  vbv.BufferLocation =
      ID3D12Resource_GetGPUVirtualAddress(fb.text_vertex_buf) + vb_byte_offset;
  vbv.SizeInBytes = vertex_count * sizeof(Vertex2D);
  vbv.StrideInBytes = sizeof(Vertex2D);
  ID3D12GraphicsCommandList_IASetVertexBuffers(cmd, 0, 1, &vbv);

  D3D12_INDEX_BUFFER_VIEW ibv = {};
  ibv.BufferLocation =
      ID3D12Resource_GetGPUVirtualAddress(fb.text_index_buf) + ib_byte_offset;
  ibv.SizeInBytes = index_count * sizeof(u32);
  ibv.Format = DXGI_FORMAT_R32_UINT;
  ID3D12GraphicsCommandList_IASetIndexBuffer(cmd, &ibv);

  ID3D12GraphicsCommandList_IASetPrimitiveTopology(
      cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ID3D12GraphicsCommandList_DrawIndexedInstanced(cmd, index_count, 1, 0, 0, 0);

  // Switch back to quad pipeline
  ID3D12GraphicsCommandList_SetPipelineState(cmd, quad_pso_);
  ID3D12GraphicsCommandList_SetGraphicsRootSignature(cmd, root_sig_);
  ID3D12GraphicsCommandList_SetDescriptorHeaps(cmd, 1, heaps);
  set_projection_constants(ds.x, ds.y);
}

// ---------------------------------------------------------------------------
// display_size / dpi_scale
// ---------------------------------------------------------------------------

Vec2 RHI::Impl::display_size() const {
  if (active_offscreen_target_ != kInvalidTexture)
    return offscreen_display_size_;
  return {static_cast<f32>(swapchain_extent_.width) / dpi_scale_,
          static_cast<f32>(swapchain_extent_.height) / dpi_scale_};
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

  // Create texture resource on DEFAULT heap
  D3D12_HEAP_PROPERTIES heap_props = {};
  heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Width = width;
  desc.Height = height;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = dxgi_format;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  desc.Flags = D3D12_RESOURCE_FLAG_NONE;

  HRESULT hr = ID3D12Device_CreateCommittedResource(
      device_, &heap_props, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, &IID_ID3D12Resource,
      reinterpret_cast<void**>(&slot.resource));
  if (FAILED(hr)) {
    std::fprintf(
        stderr, "ultragui-d3d12: CreateTexture resource failed: 0x%08lx\n", hr);
    return kInvalidTexture;
  }

  // Upload pixel data
  upload_texture_data(slot.resource, width, height, pixel_size, pixels,
                      D3D12_RESOURCE_STATE_COPY_DEST);

  // Create SRV
  slot.srv_index = handle;  // 1:1 mapping for simplicity
  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
  srv_desc.Format = dxgi_format;
  srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Texture2D.MipLevels = 1;
  ID3D12Device_CreateShaderResourceView(device_, slot.resource, &srv_desc,
                                        srv_cpu_handle(slot.srv_index));

  slot.width = width;
  slot.height = height;
  slot.pixel_size = pixel_size;
  slot.format = dxgi_format;
  slot.in_use = true;
  return handle;
}

void RHI::Impl::UpdateTexture(RHITextureHandle handle, const void* pixels) {
  if (handle >= MAX_TEXTURES || !textures_[handle].in_use) return;

  auto& slot = textures_[handle];
  upload_texture_data(slot.resource, slot.width, slot.height, slot.pixel_size,
                      pixels, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void RHI::Impl::DestroyTexture(RHITextureHandle handle) {
  if (handle >= MAX_TEXTURES || !textures_[handle].in_use) return;
  if (textures_[handle].is_render_target) {
    DestroyRenderTarget(handle);
    return;
  }
  wait_for_gpu();
  auto& slot = textures_[handle];
  if (slot.resource) {
    ID3D12Resource_Release(slot.resource);
  }
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

  // Create texture with ALLOW_RENDER_TARGET flag
  D3D12_HEAP_PROPERTIES heap_props = {};
  heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Width = width;
  desc.Height = height;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = swapchain_dxgi_format_;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  D3D12_CLEAR_VALUE clear_val = {};
  clear_val.Format = swapchain_dxgi_format_;

  HRESULT hr = ID3D12Device_CreateCommittedResource(
      device_, &heap_props, D3D12_HEAP_FLAG_NONE, &desc,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_val,
      &IID_ID3D12Resource, reinterpret_cast<void**>(&slot.resource));
  if (FAILED(hr)) {
    std::fprintf(
        stderr, "ultragui-d3d12: CreateRenderTarget resource failed: 0x%08lx\n",
        hr);
    return kInvalidTexture;
  }

  // Create RTV
  slot.rtv_index = rtv_next_index_++;
  D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
  rtv_desc.Format = swapchain_dxgi_format_;
  rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  ID3D12Device_CreateRenderTargetView(device_, slot.resource, &rtv_desc,
                                      rtv_cpu_handle(slot.rtv_index));

  // Create SRV
  slot.srv_index = handle;
  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
  srv_desc.Format = swapchain_dxgi_format_;
  srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Texture2D.MipLevels = 1;
  ID3D12Device_CreateShaderResourceView(device_, slot.resource, &srv_desc,
                                        srv_cpu_handle(slot.srv_index));

  slot.width = width;
  slot.height = height;
  slot.pixel_size = 4;
  slot.format = swapchain_dxgi_format_;
  slot.in_use = true;
  slot.is_render_target = true;
  return handle;
}

void RHI::Impl::DestroyRenderTarget(RHITextureHandle handle) {
  if (handle >= MAX_TEXTURES || !textures_[handle].in_use ||
      !textures_[handle].is_render_target)
    return;
  wait_for_gpu();
  auto& slot = textures_[handle];
  if (slot.resource) {
    ID3D12Resource_Release(slot.resource);
  }
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
  auto* cmd = cmd_lists_[current_frame_];

  active_offscreen_target_ = target;
  offscreen_display_size_ = {static_cast<f32>(slot.width),
                             static_cast<f32>(slot.height)};

  // Transition: PIXEL_SHADER_RESOURCE -> RENDER_TARGET
  {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = slot.resource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &barrier);
  }

  // Set render target
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_cpu_handle(slot.rtv_index);
  ID3D12GraphicsCommandList_OMSetRenderTargets(cmd, 1, &rtv, FALSE, nullptr);

  // Clear
  f32 clear[4] = {clear_color.r, clear_color.g, clear_color.b, clear_color.a};
  ID3D12GraphicsCommandList_ClearRenderTargetView(cmd, rtv, clear, 0, nullptr);

  // Set viewport
  D3D12_VIEWPORT viewport = {};
  viewport.Width = static_cast<f32>(slot.width);
  viewport.Height = static_cast<f32>(slot.height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  ID3D12GraphicsCommandList_RSSetViewports(cmd, 1, &viewport);

  // Set scissor
  D3D12_RECT scissor = {};
  scissor.right = static_cast<LONG>(slot.width);
  scissor.bottom = static_cast<LONG>(slot.height);
  ID3D12GraphicsCommandList_RSSetScissorRects(cmd, 1, &scissor);

  // Set pipeline and root sig
  ID3D12GraphicsCommandList_SetPipelineState(cmd, quad_pso_);
  ID3D12GraphicsCommandList_SetGraphicsRootSignature(cmd, root_sig_);

  ID3D12DescriptorHeap* heaps[] = {srv_heap_};
  ID3D12GraphicsCommandList_SetDescriptorHeaps(cmd, 1, heaps);

  // Projection for pixel-exact offscreen coordinates
  set_projection_constants(static_cast<f32>(slot.width),
                           static_cast<f32>(slot.height));

  ID3D12GraphicsCommandList_IASetPrimitiveTopology(
      cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  return true;
}

void RHI::Impl::EndOffscreen(RHITextureHandle target) {
  if (active_offscreen_target_ == kInvalidTexture) return;

  auto& slot = textures_[target];
  auto* cmd = cmd_lists_[current_frame_];

  // Transition: RENDER_TARGET -> PIXEL_SHADER_RESOURCE
  {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = slot.resource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &barrier);
  }

  active_offscreen_target_ = kInvalidTexture;

  // Restore swapchain render target and state
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_cpu_handle(image_index_);
  ID3D12GraphicsCommandList_OMSetRenderTargets(cmd, 1, &rtv, FALSE, nullptr);

  // Restore viewport/scissor
  D3D12_VIEWPORT viewport = {};
  viewport.Width = static_cast<f32>(swapchain_extent_.width);
  viewport.Height = static_cast<f32>(swapchain_extent_.height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  ID3D12GraphicsCommandList_RSSetViewports(cmd, 1, &viewport);

  D3D12_RECT scissor = {};
  scissor.right = static_cast<LONG>(swapchain_extent_.width);
  scissor.bottom = static_cast<LONG>(swapchain_extent_.height);
  ID3D12GraphicsCommandList_RSSetScissorRects(cmd, 1, &scissor);

  // Restore pipeline state
  ID3D12GraphicsCommandList_SetPipelineState(cmd, quad_pso_);
  ID3D12GraphicsCommandList_SetGraphicsRootSignature(cmd, root_sig_);

  ID3D12DescriptorHeap* heaps[] = {srv_heap_};
  ID3D12GraphicsCommandList_SetDescriptorHeaps(cmd, 1, heaps);

  f32 win_w = static_cast<f32>(swapchain_extent_.width) / dpi_scale_;
  f32 win_h = static_cast<f32>(swapchain_extent_.height) / dpi_scale_;
  set_projection_constants(win_w, win_h);

  ID3D12GraphicsCommandList_IASetPrimitiveTopology(
      cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ---------------------------------------------------------------------------
// Video frame conversion (YCbCr -> RGBA)
// ---------------------------------------------------------------------------

void RHI::Impl::ConvertVideoFrame(RHITextureHandle target, RHITextureHandle y,
                                  RHITextureHandle cb, RHITextureHandle cr) {
  if (!ensure_video_pipeline()) return;
  if (target >= MAX_TEXTURES || !textures_[target].in_use ||
      !textures_[target].is_render_target)
    return;
  if (y >= MAX_TEXTURES || !textures_[y].in_use) return;
  if (cb >= MAX_TEXTURES || !textures_[cb].in_use) return;
  if (cr >= MAX_TEXTURES || !textures_[cr].in_use) return;

  auto& slot = textures_[target];
  auto* cmd = cmd_lists_[current_frame_];

  // Transition target: PIXEL_SHADER_RESOURCE -> RENDER_TARGET
  {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = slot.resource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &barrier);
  }

  // Set render target
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_cpu_handle(slot.rtv_index);
  ID3D12GraphicsCommandList_OMSetRenderTargets(cmd, 1, &rtv, FALSE, nullptr);

  f32 clear[4] = {0, 0, 0, 0};
  ID3D12GraphicsCommandList_ClearRenderTargetView(cmd, rtv, clear, 0, nullptr);

  // Set video pipeline
  ID3D12GraphicsCommandList_SetPipelineState(cmd, video_pso_);
  ID3D12GraphicsCommandList_SetGraphicsRootSignature(cmd, video_root_sig_);

  ID3D12DescriptorHeap* heaps[] = {srv_heap_};
  ID3D12GraphicsCommandList_SetDescriptorHeaps(cmd, 1, heaps);

  // Set 3 SRV descriptor tables for Y, Cb, Cr
  ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(
      cmd, 0, srv_gpu_handle(textures_[y].srv_index));
  ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(
      cmd, 1, srv_gpu_handle(textures_[cb].srv_index));
  ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(
      cmd, 2, srv_gpu_handle(textures_[cr].srv_index));

  // Viewport and scissor
  D3D12_VIEWPORT viewport = {};
  viewport.Width = static_cast<f32>(slot.width);
  viewport.Height = static_cast<f32>(slot.height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  ID3D12GraphicsCommandList_RSSetViewports(cmd, 1, &viewport);

  D3D12_RECT scissor = {};
  scissor.right = static_cast<LONG>(slot.width);
  scissor.bottom = static_cast<LONG>(slot.height);
  ID3D12GraphicsCommandList_RSSetScissorRects(cmd, 1, &scissor);

  ID3D12GraphicsCommandList_IASetPrimitiveTopology(
      cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Draw fullscreen triangle (3 vertices, no vertex buffer)
  ID3D12GraphicsCommandList_DrawInstanced(cmd, 3, 1, 0, 0);

  // Transition target: RENDER_TARGET -> PIXEL_SHADER_RESOURCE
  {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = slot.resource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &barrier);
  }

  // Restore quad pipeline state (caller expects it active)
  ID3D12GraphicsCommandList_SetPipelineState(cmd, quad_pso_);
  ID3D12GraphicsCommandList_SetGraphicsRootSignature(cmd, root_sig_);
  ID3D12GraphicsCommandList_SetDescriptorHeaps(cmd, 1, heaps);
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void RHI::Impl::Shutdown() {
  s_rhi_instance = nullptr;

  if (device_ && fence_) wait_for_gpu();

  // Textures
  for (u32 i = 0; i < MAX_TEXTURES; ++i) {
    if (textures_[i].in_use) {
      if (textures_[i].is_render_target)
        DestroyRenderTarget(i);
      else
        DestroyTexture(i);
    }
  }

  // Per-frame upload buffers
  for (u32 i = 0; i < MAX_FRAMES; ++i) {
    auto& fb = frame_bufs_[i];
    if (fb.vertex_buf) {
      ID3D12Resource_Unmap(fb.vertex_buf, 0, nullptr);
      ID3D12Resource_Release(fb.vertex_buf);
    }
    if (fb.index_buf) {
      ID3D12Resource_Unmap(fb.index_buf, 0, nullptr);
      ID3D12Resource_Release(fb.index_buf);
    }
    if (fb.text_vertex_buf) {
      ID3D12Resource_Unmap(fb.text_vertex_buf, 0, nullptr);
      ID3D12Resource_Release(fb.text_vertex_buf);
    }
    if (fb.text_index_buf) {
      ID3D12Resource_Unmap(fb.text_index_buf, 0, nullptr);
      ID3D12Resource_Release(fb.text_index_buf);
    }
  }

  // Command resources
  for (u32 i = 0; i < MAX_FRAMES; ++i) {
    if (cmd_lists_[i]) ID3D12GraphicsCommandList_Release(cmd_lists_[i]);
    if (cmd_allocs_[i]) ID3D12CommandAllocator_Release(cmd_allocs_[i]);
  }
  if (upload_cmd_list_) ID3D12GraphicsCommandList_Release(upload_cmd_list_);
  if (upload_cmd_alloc_) ID3D12CommandAllocator_Release(upload_cmd_alloc_);
  if (fence_) ID3D12Fence_Release(fence_);
  if (cmd_queue_) ID3D12CommandQueue_Release(cmd_queue_);

  // Pipelines
  if (video_pso_) ID3D12PipelineState_Release(video_pso_);
  if (video_root_sig_) ID3D12RootSignature_Release(video_root_sig_);
  if (text_pso_) ID3D12PipelineState_Release(text_pso_);
  if (quad_pso_) ID3D12PipelineState_Release(quad_pso_);
  if (root_sig_) ID3D12RootSignature_Release(root_sig_);

  // Descriptor heaps
  if (srv_heap_) ID3D12DescriptorHeap_Release(srv_heap_);
  if (rtv_heap_) ID3D12DescriptorHeap_Release(rtv_heap_);

  // Swapchain resources
  cleanup_swapchain();

  // Vulkan acquire fence
  if (vk_acquire_fence_ && vk_device_)
    vkDestroyFence(vk_device_, vk_acquire_fence_, nullptr);

  // Vulkan surface
  if (vk_surface_ && vk_instance_)
    vkDestroySurfaceKHR(vk_instance_, vk_surface_, nullptr);

  // D3D12 device (releases the underlying VkDevice)
  if (device_) ID3D12Device_Release(device_);

  // vkd3d instance (releases VkInstance)
  if (vkd3d_inst_) vkd3d_instance_decref(vkd3d_inst_);
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
