#ifndef ULTRAGUI_BACKENDS_UGUI_IMPL_VULKAN_H_
#define ULTRAGUI_BACKENDS_UGUI_IMPL_VULKAN_H_

// ugui_impl_vulkan: Vulkan renderer backend for ultragui, modeled on
// Dear ImGui's imgui_impl_vulkan.
//
// The HOST owns everything: VkInstance/VkDevice/VkQueue, the swapchain, the
// render pass, the per-frame command buffer, and presentation. This backend
// only creates its pipelines/descriptors/vertex buffers and records ultragui's
// draw data into a command buffer YOU hand it, inside a render pass YOU began.
//
// Typical per-frame use, inside your own render loop:
//
//   ugui::vk::NewFrame();
//   const ugui::DrawData& dd = ui.RenderDrawData();   // produce geometry
//   // ... your scene rendering ...
//   vkCmdBeginRenderPass(cmd, ...);                   // your pass
//   ugui::vk::RenderDrawData(dd, cmd);                // ui on top
//   vkCmdEndRenderPass(cmd);
//
// Font atlas: call UpdateFontAtlas() whenever TextEngine::atlas_revision()
// changes (it grows as glyphs are shaped), passing TextEngine::atlas_pixels().

#include <vulkan/vulkan.h>

#include <ugui/core/types.h>
#include <ugui/render/draw_data.h>

namespace ugui {
namespace vk {

/// Everything the backend needs from the host's Vulkan setup. Mirrors
/// ImGui_ImplVulkan_InitInfo.
struct InitInfo {
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  u32 queue_family = 0;
  VkQueue queue = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;  // host-owned, big enough
  VkRenderPass render_pass = VK_NULL_HANDLE;          // pass cmds record into
  u32 subpass = 0;
  u32 frames_in_flight = 2;  // ring size for per-frame vertex/index buffers
  VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
  const VkAllocationCallbacks* allocator = nullptr;

  /// Directory containing ultragui's compiled SPIR-V (quad.vert.spv,
  /// quad.frag.spv, text.frag.spv). The same shaders the Vulkan RHI uses.
  const char* shader_dir = nullptr;
};

/// Create pipelines, descriptor set layout, sampler and per-frame buffers.
/// Returns false on failure. Does not touch the swapchain.
bool Init(const InitInfo& info);

/// Destroy all backend Vulkan objects. Does not destroy host-owned handles.
void Shutdown();

/// Call once at the start of each frame, before RenderDrawData.
void NewFrame();

/// (Re)upload the glyph atlas as the backend's font texture. Call from Init
/// and whenever TextEngine::atlas_revision() changed. `pixels` is R8/alpha8,
/// width*height bytes (see TextEngine::atlas_pixels()/atlas_size()).
bool UpdateFontAtlas(const u8* pixels, u32 width, u32 height);

/// Record draw commands for `draw_data` into `command_buffer`. Must be called
/// inside a render pass compatible with InitInfo::render_pass that the host
/// already began on this command buffer.
void RenderDrawData(const DrawData& draw_data, VkCommandBuffer command_buffer);

}  // namespace vk
}  // namespace ugui

#endif  // ULTRAGUI_BACKENDS_UGUI_IMPL_VULKAN_H_
