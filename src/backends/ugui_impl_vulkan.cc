// ugui_impl_vulkan: Vulkan renderer backend for ultragui (see the header for
// the integration contract). Mirrors the pipeline/vertex/descriptor setup of
// the bundled Vulkan RHI so it renders ultragui::DrawData identically, but into
// a command buffer and render pass owned by the host application.

#include <ultragui/backends/ugui_impl_vulkan.h>
#include <ultragui/render/vertex.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace ugui {
namespace vk {
namespace {

struct GpuBuffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize capacity = 0;
};

struct FrameBuffers {
  GpuBuffer quad_vtx, quad_idx, text_vtx, text_idx;
};

struct Texture {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkDescriptorSet set = VK_NULL_HANDLE;
};

struct Backend {
  InitInfo info{};
  VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline quad_pipeline = VK_NULL_HANDLE;
  VkPipeline text_pipeline = VK_NULL_HANDLE;
  VkSampler linear_sampler = VK_NULL_HANDLE;
  VkSampler nearest_sampler = VK_NULL_HANDLE;
  Texture white;
  Texture font;
  u32 font_revision = ~0u;
  std::vector<FrameBuffers> frames;
  u32 frame_index = 0;
};

Backend g;

u32 FindMemoryType(u32 type_filter, VkMemoryPropertyFlags props) {
  VkPhysicalDeviceMemoryProperties mp;
  vkGetPhysicalDeviceMemoryProperties(g.info.physical_device, &mp);
  for (u32 i = 0; i < mp.memoryTypeCount; ++i)
    if ((type_filter & (1u << i)) &&
        (mp.memoryTypes[i].propertyFlags & props) == props)
      return i;
  return 0;
}

void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags props, GpuBuffer& out) {
  VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  ci.size = size;
  ci.usage = usage;
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vkCreateBuffer(g.info.device, &ci, g.info.allocator, &out.buffer);
  VkMemoryRequirements reqs;
  vkGetBufferMemoryRequirements(g.info.device, out.buffer, &reqs);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = reqs.size;
  ai.memoryTypeIndex = FindMemoryType(reqs.memoryTypeBits, props);
  vkAllocateMemory(g.info.device, &ai, g.info.allocator, &out.memory);
  vkBindBufferMemory(g.info.device, out.buffer, out.memory, 0);
  out.capacity = size;
}

void DestroyBuffer(GpuBuffer& b) {
  if (b.buffer) vkDestroyBuffer(g.info.device, b.buffer, g.info.allocator);
  if (b.memory) vkFreeMemory(g.info.device, b.memory, g.info.allocator);
  b = {};
}

// Upload `bytes` from `src` into a host-visible buffer, growing it if needed.
void UploadBuffer(GpuBuffer& b, VkBufferUsageFlags usage, const void* src,
                  VkDeviceSize bytes) {
  if (bytes == 0) return;
  if (b.capacity < bytes) {
    DestroyBuffer(b);
    VkDeviceSize cap = std::max<VkDeviceSize>(bytes, 4096);
    CreateBuffer(cap, usage,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 b);
  }
  void* dst = nullptr;
  vkMapMemory(g.info.device, b.memory, 0, bytes, 0, &dst);
  std::memcpy(dst, src, static_cast<size_t>(bytes));
  vkUnmapMemory(g.info.device, b.memory);
}

std::vector<char> ReadFile(const std::string& path) {
  std::ifstream f(path, std::ios::ate | std::ios::binary);
  if (!f) return {};
  size_t size = static_cast<size_t>(f.tellg());
  std::vector<char> buf(size);
  f.seekg(0);
  f.read(buf.data(), static_cast<std::streamsize>(size));
  return buf;
}

VkShaderModule LoadShader(const char* name) {
  std::string dir = g.info.shader_dir ? g.info.shader_dir : ".";
  auto code = ReadFile(dir + "/" + name);
  if (code.empty()) {
    std::fprintf(stderr, "ugui_impl_vulkan: missing shader %s/%s\n",
                 dir.c_str(), name);
    return VK_NULL_HANDLE;
  }
  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = code.size();
  ci.pCode = reinterpret_cast<const u32*>(code.data());
  VkShaderModule m = VK_NULL_HANDLE;
  vkCreateShaderModule(g.info.device, &ci, g.info.allocator, &m);
  return m;
}

VkPipeline CreatePipeline(const char* vert, const char* frag) {
  VkShaderModule vs = LoadShader(vert);
  VkShaderModule fs = LoadShader(frag);
  if (!vs || !fs) return VK_NULL_HANDLE;

  VkPipelineShaderStageCreateInfo stages[2] = {};
  stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vs;
  stages[0].pName = "main";
  stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fs;
  stages[1].pName = "main";

  VkVertexInputBindingDescription binding{};
  binding.binding = 0;
  binding.stride = sizeof(Vertex2D);
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attrs[9] = {};
  attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, pos)};
  attrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, uv)};
  attrs[2] = {2, 0, VK_FORMAT_R32_UINT, offsetof(Vertex2D, color)};
  attrs[3] = {3, 0, VK_FORMAT_R32_UINT, offsetof(Vertex2D, color2)};
  attrs[4] = {4, 0, VK_FORMAT_R32_UINT, offsetof(Vertex2D, corner_radii)};
  attrs[5] = {5, 0, VK_FORMAT_R32_SFLOAT, offsetof(Vertex2D, softness)};
  attrs[6] = {6, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, half_size)};
  attrs[7] = {7, 0, VK_FORMAT_R32_SFLOAT, offsetof(Vertex2D, border_width)};
  attrs[8] = {8, 0, VK_FORMAT_R32_UINT, offsetof(Vertex2D, border_color)};

  VkPipelineVertexInputStateCreateInfo vin{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vin.vertexBindingDescriptionCount = 1;
  vin.pVertexBindingDescriptions = &binding;
  vin.vertexAttributeDescriptionCount = 9;
  vin.pVertexAttributeDescriptions = attrs;

  VkPipelineInputAssemblyStateCreateInfo ia{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo vp{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = 1;
  vp.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rs{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.lineWidth = 1.0f;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo ms{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = g.info.msaa_samples;

  VkPipelineColorBlendAttachmentState ba{};
  ba.blendEnable = VK_TRUE;
  ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  ba.colorBlendOp = VK_BLEND_OP_ADD;
  ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  ba.alphaBlendOp = VK_BLEND_OP_ADD;
  ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo cb{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.attachmentCount = 1;
  cb.pAttachments = &ba;

  VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo ds{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  ds.dynamicStateCount = 2;
  ds.pDynamicStates = dyn;

  VkGraphicsPipelineCreateInfo pi{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pi.stageCount = 2;
  pi.pStages = stages;
  pi.pVertexInputState = &vin;
  pi.pInputAssemblyState = &ia;
  pi.pViewportState = &vp;
  pi.pRasterizationState = &rs;
  pi.pMultisampleState = &ms;
  pi.pColorBlendState = &cb;
  pi.pDynamicState = &ds;
  pi.layout = g.pipeline_layout;
  pi.renderPass = g.info.render_pass;
  pi.subpass = g.info.subpass;

  VkPipeline pipeline = VK_NULL_HANDLE;
  vkCreateGraphicsPipelines(g.info.device, g.info.pipeline_cache, 1, &pi,
                            g.info.allocator, &pipeline);
  vkDestroyShaderModule(g.info.device, vs, g.info.allocator);
  vkDestroyShaderModule(g.info.device, fs, g.info.allocator);
  return pipeline;
}

void OneTimeSubmit(const std::function<void(VkCommandBuffer)>& fn) {
  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.queueFamilyIndex = g.info.queue_family;
  pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  VkCommandPool pool;
  vkCreateCommandPool(g.info.device, &pci, g.info.allocator, &pool);
  VkCommandBufferAllocateInfo cai{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cai.commandPool = pool;
  cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cai.commandBufferCount = 1;
  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(g.info.device, &cai, &cmd);
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &bi);
  fn(cmd);
  vkEndCommandBuffer(cmd);
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;
  vkQueueSubmit(g.info.queue, 1, &si, VK_NULL_HANDLE);
  vkQueueWaitIdle(g.info.queue);
  vkDestroyCommandPool(g.info.device, pool, g.info.allocator);
}

// Create a sampled texture from CPU pixels (RGBA8 or R8) + its descriptor set.
Texture CreateTexture(u32 w, u32 h, VkFormat fmt, u32 pixel_size,
                      const void* pixels, VkSampler sampler) {
  Texture t;
  VkDeviceSize size = static_cast<VkDeviceSize>(w) * h * pixel_size;

  GpuBuffer staging;
  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging);
  void* data = nullptr;
  vkMapMemory(g.info.device, staging.memory, 0, size, 0, &data);
  std::memcpy(data, pixels, static_cast<size_t>(size));
  vkUnmapMemory(g.info.device, staging.memory);

  VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ici.imageType = VK_IMAGE_TYPE_2D;
  ici.extent = {w, h, 1};
  ici.mipLevels = 1;
  ici.arrayLayers = 1;
  ici.format = fmt;
  ici.tiling = VK_IMAGE_TILING_OPTIMAL;
  ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  ici.samples = VK_SAMPLE_COUNT_1_BIT;
  vkCreateImage(g.info.device, &ici, g.info.allocator, &t.image);

  VkMemoryRequirements reqs;
  vkGetImageMemoryRequirements(g.info.device, t.image, &reqs);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = reqs.size;
  ai.memoryTypeIndex =
      FindMemoryType(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vkAllocateMemory(g.info.device, &ai, g.info.allocator, &t.memory);
  vkBindImageMemory(g.info.device, t.image, t.memory, 0);

  OneTimeSubmit([&](VkCommandBuffer cmd) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = t.image;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &b);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {w, h, 1};
    vkCmdCopyBufferToImage(cmd, staging.buffer, t.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &b);
  });
  DestroyBuffer(staging);

  VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vci.image = t.image;
  vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vci.format = fmt;
  vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCreateImageView(g.info.device, &vci, g.info.allocator, &t.view);

  VkDescriptorSetAllocateInfo dai{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  dai.descriptorPool = g.info.descriptor_pool;
  dai.descriptorSetCount = 1;
  dai.pSetLayouts = &g.set_layout;
  vkAllocateDescriptorSets(g.info.device, &dai, &t.set);

  VkDescriptorImageInfo dii{};
  dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  dii.imageView = t.view;
  dii.sampler = sampler;
  VkWriteDescriptorSet wd{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  wd.dstSet = t.set;
  wd.dstBinding = 0;
  wd.descriptorCount = 1;
  wd.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  wd.pImageInfo = &dii;
  vkUpdateDescriptorSets(g.info.device, 1, &wd, 0, nullptr);
  return t;
}

void DestroyTexture(Texture& t) {
  if (t.view) vkDestroyImageView(g.info.device, t.view, g.info.allocator);
  if (t.image) vkDestroyImage(g.info.device, t.image, g.info.allocator);
  if (t.memory) vkFreeMemory(g.info.device, t.memory, g.info.allocator);
  t = {};
}

VkSampler MakeSampler(VkFilter filter) {
  VkSamplerCreateInfo s{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  s.magFilter = filter;
  s.minFilter = filter;
  s.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  s.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  VkSampler out = VK_NULL_HANDLE;
  vkCreateSampler(g.info.device, &s, g.info.allocator, &out);
  return out;
}

}  // namespace

bool Init(const InitInfo& info) {
  g.info = info;
  if (g.info.frames_in_flight < 1) g.info.frames_in_flight = 2;

  VkDescriptorSetLayoutBinding b{};
  b.binding = 0;
  b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  b.descriptorCount = 1;
  b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo lci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  lci.bindingCount = 1;
  lci.pBindings = &b;
  if (vkCreateDescriptorSetLayout(g.info.device, &lci, g.info.allocator,
                                  &g.set_layout) != VK_SUCCESS)
    return false;

  VkPushConstantRange pcr{};
  pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pcr.offset = 0;
  pcr.size = sizeof(f32) * 4;
  VkPipelineLayoutCreateInfo plci{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  plci.setLayoutCount = 1;
  plci.pSetLayouts = &g.set_layout;
  plci.pushConstantRangeCount = 1;
  plci.pPushConstantRanges = &pcr;
  if (vkCreatePipelineLayout(g.info.device, &plci, g.info.allocator,
                             &g.pipeline_layout) != VK_SUCCESS)
    return false;

  g.linear_sampler = MakeSampler(VK_FILTER_LINEAR);
  g.nearest_sampler = MakeSampler(VK_FILTER_NEAREST);

  g.quad_pipeline = CreatePipeline("quad.vert.spv", "quad.frag.spv");
  g.text_pipeline = CreatePipeline("text.vert.spv", "text.frag.spv");
  if (!g.quad_pipeline || !g.text_pipeline) return false;

  u32 white = 0xFFFFFFFFu;
  g.white = CreateTexture(1, 1, VK_FORMAT_R8G8B8A8_UNORM, 4, &white,
                          g.linear_sampler);

  g.frames.resize(g.info.frames_in_flight);
  return true;
}

void Shutdown() {
  if (!g.info.device) return;
  vkDeviceWaitIdle(g.info.device);
  for (auto& f : g.frames) {
    DestroyBuffer(f.quad_vtx);
    DestroyBuffer(f.quad_idx);
    DestroyBuffer(f.text_vtx);
    DestroyBuffer(f.text_idx);
  }
  g.frames.clear();
  DestroyTexture(g.font);
  DestroyTexture(g.white);
  if (g.quad_pipeline)
    vkDestroyPipeline(g.info.device, g.quad_pipeline, g.info.allocator);
  if (g.text_pipeline)
    vkDestroyPipeline(g.info.device, g.text_pipeline, g.info.allocator);
  if (g.pipeline_layout)
    vkDestroyPipelineLayout(g.info.device, g.pipeline_layout, g.info.allocator);
  if (g.set_layout)
    vkDestroyDescriptorSetLayout(g.info.device, g.set_layout, g.info.allocator);
  if (g.linear_sampler)
    vkDestroySampler(g.info.device, g.linear_sampler, g.info.allocator);
  if (g.nearest_sampler)
    vkDestroySampler(g.info.device, g.nearest_sampler, g.info.allocator);
  g = {};
}

void NewFrame() {
  g.frame_index = (g.frame_index + 1) % g.info.frames_in_flight;
}

bool UpdateFontAtlas(const u8* pixels, u32 width, u32 height) {
  if (!pixels || width == 0 || height == 0) return false;
  vkDeviceWaitIdle(g.info.device);
  DestroyTexture(g.font);
  g.font = CreateTexture(width, height, VK_FORMAT_R8_UNORM, 1, pixels,
                         g.nearest_sampler);
  return g.font.image != VK_NULL_HANDLE;
}

void RenderDrawData(const DrawData& dd, VkCommandBuffer cmd) {
  if (!dd.valid || dd.command_count == 0) return;
  if (dd.display_size.x <= 0.0f || dd.display_size.y <= 0.0f) return;

  FrameBuffers& fb = g.frames[g.frame_index];
  UploadBuffer(fb.quad_vtx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, dd.quad_vertices,
               dd.quad_vertex_count * sizeof(Vertex2D));
  UploadBuffer(fb.quad_idx, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, dd.quad_indices,
               dd.quad_index_count * sizeof(u32));
  UploadBuffer(fb.text_vtx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, dd.text_vertices,
               dd.text_vertex_count * sizeof(Vertex2D));
  UploadBuffer(fb.text_idx, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, dd.text_indices,
               dd.text_index_count * sizeof(u32));

  f32 sx = dd.framebuffer_scale.x > 0 ? dd.framebuffer_scale.x : 1.0f;
  f32 sy = dd.framebuffer_scale.y > 0 ? dd.framebuffer_scale.y : 1.0f;

  VkViewport vp{};
  vp.x = 0;
  vp.y = 0;
  vp.width = dd.display_size.x * sx;
  vp.height = dd.display_size.y * sy;
  vp.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &vp);

  f32 push[4] = {2.0f / dd.display_size.x, 2.0f / dd.display_size.y, -1.0f,
                 -1.0f};
  vkCmdPushConstants(cmd, g.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(push), push);

  VkPipeline bound_pipeline = VK_NULL_HANDLE;
  for (u32 i = 0; i < dd.command_count; ++i) {
    const DrawCmd& c = dd.commands[i];
    if (c.elem_count == 0) continue;

    VkPipeline want = c.is_text ? g.text_pipeline : g.quad_pipeline;
    if (want != bound_pipeline) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, want);
      bound_pipeline = want;
    }

    VkDescriptorSet set = (c.is_text || c.texture_id == kFontTextureId)
                              ? g.font.set
                              : g.white.set;
    if (set == VK_NULL_HANDLE) set = g.white.set;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            g.pipeline_layout, 0, 1, &set, 0, nullptr);

    // Scissor in framebuffer pixels, clamped to >= 0.
    VkRect2D scissor{};
    f32 x0 = std::max(0.0f, c.clip_rect.x) * sx;
    f32 y0 = std::max(0.0f, c.clip_rect.y) * sy;
    f32 x1 = std::max(0.0f, c.clip_rect.x + c.clip_rect.w) * sx;
    f32 y1 = std::max(0.0f, c.clip_rect.y + c.clip_rect.h) * sy;
    scissor.offset = {static_cast<i32>(x0), static_cast<i32>(y0)};
    scissor.extent = {static_cast<u32>(std::max(0.0f, x1 - x0)),
                      static_cast<u32>(std::max(0.0f, y1 - y0))};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    GpuBuffer& vb = c.is_text ? fb.text_vtx : fb.quad_vtx;
    GpuBuffer& ib = c.is_text ? fb.text_idx : fb.quad_idx;
    if (!vb.buffer || !ib.buffer) continue;
    VkDeviceSize zero = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb.buffer, &zero);
    vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, c.elem_count, 1, c.index_offset, 0, 0);
  }
}

}  // namespace vk
}  // namespace ugui
