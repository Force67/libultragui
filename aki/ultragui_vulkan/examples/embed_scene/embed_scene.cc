// embed_scene (C++): the reference for src/main.aki. A host that owns its
// Vulkan device, swapchain, render pass, command buffers and presentation
// (the examples/embed_vulkan approach) records ultragui draw data through
// ugui::vk (ugui_impl_vulkan.cc). With no UIContext ported yet, the DrawData
// is built by hand: quads (radii, border, shadow, gradient), user textures
// through the TextureBackend (RGBA linear, BGRA nearest, one updated each
// frame, one destroyed), text with the glyph atlas uploaded by
// UpdateFontAtlas, the atlas sampled as an image (kFontTextureId), an unknown
// id (white fallback), clip rects, and a blur command the backend skips.
// The aki program builds the same draw data; under aki/tools/xshot.sh the
// two screenshots should be equal.
//
//   embed_scene <shader_dir> <font.ttf> [frames]
//
// Build: aki/ultragui_vulkan/examples/build_cpp.sh embed_scene <ref-build-dir>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <ugui/backends/ugui_impl_vulkan.h>
#include <ugui/render/draw_data.h>
#include <ugui/text/text_engine.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

using namespace ugui;

namespace {

const int kFramesInFlight = 2;

void Die(const char* msg) {
  fprintf(stderr, "embed_scene: %s\n", msg);
  exit(1);
}
#define VKCHECK(x) \
  if ((x) != VK_SUCCESS) Die(#x)

struct Mesh {
  std::vector<Vertex2D> v;
  std::vector<u32> i;
};

// Renderer2D::EmitQuad at dpi 1 with no transform.
void Quad(Mesh& m, Rect r, Color c, Color c2, u32 radii, f32 softness,
          f32 border_width, Color border) {
  u32 color = Vertex2D::PackColor(c.r, c.g, c.b, c.a);
  u32 color2 = Vertex2D::PackColor(c2.r, c2.g, c2.b, c2.a);
  u32 bc = Vertex2D::PackColor(border.r, border.g, border.b, border.a);
  f32 hw = r.w * 0.5f;
  f32 hh = r.h * 0.5f;
  u32 base = static_cast<u32>(m.v.size());
  m.v.push_back({{r.x, r.y}, {0, 0}, color, color2, radii, softness, {hw, hh},
                 border_width, bc});
  m.v.push_back({{r.x + r.w, r.y}, {1, 0}, color, color2, radii, softness,
                 {hw, hh}, border_width, bc});
  m.v.push_back({{r.x + r.w, r.y + r.h}, {1, 1}, color, color2, radii,
                 softness, {hw, hh}, border_width, bc});
  m.v.push_back({{r.x, r.y + r.h}, {0, 1}, color, color2, radii, softness,
                 {hw, hh}, border_width, bc});
  u32 idx[6] = {0, 1, 2, 0, 2, 3};
  for (u32 k : idx) m.i.push_back(base + k);
}

void Solid(Mesh& m, Rect r, Color c, u32 radii) {
  Quad(m, r, c, c, radii, 0.0f, 0.0f, Color{0, 0, 0, 0});
}

// Renderer2D::DrawText at dpi 1 with no transform.
void Text(Mesh& m, Vec2 pos, const TextRun& run, Color color) {
  u32 packed = Vertex2D::PackColor(color.r, color.g, color.b, color.a);
  f32 cursor_x = pos.x;
  f32 baseline_y = pos.y + run.ascent;
  for (u32 i = 0; i < run.glyph_count; ++i) {
    const auto& g = run.glyphs[i];
    if (g.bmp_w <= 0 || g.bmp_h <= 0) {
      cursor_x += g.x_advance;
      continue;
    }
    f32 x = roundf(cursor_x + g.bearing_x + g.x_offset);
    f32 y = roundf(baseline_y - g.bearing_y + g.y_offset);
    f32 w = roundf(g.bmp_w);
    f32 h = roundf(g.bmp_h);
    u32 base = static_cast<u32>(m.v.size());
    m.v.push_back({{x, y}, {g.u0, g.v0}, packed, packed, 0, 0, {0, 0}, 0, 0});
    m.v.push_back(
        {{x + w, y}, {g.u1, g.v0}, packed, packed, 0, 0, {0, 0}, 0, 0});
    m.v.push_back(
        {{x + w, y + h}, {g.u1, g.v1}, packed, packed, 0, 0, {0, 0}, 0, 0});
    m.v.push_back(
        {{x, y + h}, {g.u0, g.v1}, packed, packed, 0, 0, {0, 0}, 0, 0});
    u32 idx[6] = {0, 1, 2, 0, 2, 3};
    for (u32 k : idx) m.i.push_back(base + k);
    cursor_x += g.x_advance;
  }
}

DrawCmd Cmd(Rect clip, TextureId tex, u32 first, u32 count, bool is_text,
            f32 blur = 0.0f) {
  DrawCmd c;
  c.clip_rect = clip;
  c.texture_id = tex;
  c.index_offset = first;
  c.elem_count = count;
  c.is_text = is_text;
  c.blur = blur;
  return c;
}

struct Host {
  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice phys = VK_NULL_HANDLE;
  uint32_t qfam = 0;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  VkSurfaceFormatKHR format{};
  VkRenderPass render_pass = VK_NULL_HANDLE;
  VkCommandPool cmd_pool = VK_NULL_HANDLE;
  VkDescriptorPool desc_pool = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkExtent2D extent{};
  std::vector<VkImageView> views;
  std::vector<VkFramebuffer> framebuffers;
  std::vector<VkSemaphore> render_finished;
  VkCommandBuffer cmd[kFramesInFlight]{};
  VkSemaphore image_available[kFramesInFlight]{};
  VkFence in_flight[kFramesInFlight]{};
  uint32_t frame = 0;
};
Host host;

void CreateSwapchain(GLFWwindow* win) {
  VkSurfaceCapabilitiesKHR caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(host.phys, host.surface, &caps);
  host.extent = caps.currentExtent;
  if (host.extent.width == 0xFFFFFFFFu) {
    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    host.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
  }
  uint32_t img_count = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && img_count > caps.maxImageCount)
    img_count = caps.maxImageCount;

  VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  ci.surface = host.surface;
  ci.minImageCount = img_count;
  ci.imageFormat = host.format.format;
  ci.imageColorSpace = host.format.colorSpace;
  ci.imageExtent = host.extent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.preTransform = caps.currentTransform;
  ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  ci.clipped = VK_TRUE;
  VKCHECK(vkCreateSwapchainKHR(host.device, &ci, nullptr, &host.swapchain));

  uint32_t n = 0;
  vkGetSwapchainImagesKHR(host.device, host.swapchain, &n, nullptr);
  std::vector<VkImage> images(n);
  vkGetSwapchainImagesKHR(host.device, host.swapchain, &n, images.data());
  host.views.resize(n);
  host.framebuffers.resize(n);
  host.render_finished.resize(n);
  for (uint32_t i = 0; i < n; ++i) {
    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = images[i];
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = host.format.format;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VKCHECK(vkCreateImageView(host.device, &vci, nullptr, &host.views[i]));
    VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = host.render_pass;
    fci.attachmentCount = 1;
    fci.pAttachments = &host.views[i];
    fci.width = host.extent.width;
    fci.height = host.extent.height;
    fci.layers = 1;
    VKCHECK(vkCreateFramebuffer(host.device, &fci, nullptr, &host.framebuffers[i]));
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VKCHECK(vkCreateSemaphore(host.device, &sci, nullptr, &host.render_finished[i]));
  }
}

void DestroySwapchain() {
  for (auto fb : host.framebuffers) vkDestroyFramebuffer(host.device, fb, nullptr);
  for (auto v : host.views) vkDestroyImageView(host.device, v, nullptr);
  for (auto s : host.render_finished) vkDestroySemaphore(host.device, s, nullptr);
  host.framebuffers.clear();
  host.views.clear();
  host.render_finished.clear();
  if (host.swapchain) vkDestroySwapchainKHR(host.device, host.swapchain, nullptr);
  host.swapchain = VK_NULL_HANDLE;
}

void HostInit(GLFWwindow* win) {
  uint32_t ext_count = 0;
  const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "embed_scene";
  app.apiVersion = VK_API_VERSION_1_1;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo = &app;
  ici.enabledExtensionCount = ext_count;
  ici.ppEnabledExtensionNames = exts;
  VKCHECK(vkCreateInstance(&ici, nullptr, &host.instance));
  VKCHECK(glfwCreateWindowSurface(host.instance, win, nullptr, &host.surface));

  uint32_t dev_count = 0;
  vkEnumeratePhysicalDevices(host.instance, &dev_count, nullptr);
  std::vector<VkPhysicalDevice> devs(dev_count);
  vkEnumeratePhysicalDevices(host.instance, &dev_count, devs.data());
  for (auto d : devs) {
    uint32_t qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(d, &qn, nullptr);
    std::vector<VkQueueFamilyProperties> qprops(qn);
    vkGetPhysicalDeviceQueueFamilyProperties(d, &qn, qprops.data());
    for (uint32_t i = 0; i < qn; ++i) {
      VkBool32 present = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(d, i, host.surface, &present);
      if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
        host.phys = d;
        host.qfam = i;
        break;
      }
    }
    if (host.phys) break;
  }
  if (!host.phys) Die("no suitable GPU");

  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = host.qfam;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;
  const char* dev_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.enabledExtensionCount = 1;
  dci.ppEnabledExtensionNames = dev_exts;
  VKCHECK(vkCreateDevice(host.phys, &dci, nullptr, &host.device));
  vkGetDeviceQueue(host.device, host.qfam, 0, &host.queue);

  uint32_t fmt_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(host.phys, host.surface, &fmt_count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(fmt_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(host.phys, host.surface, &fmt_count,
                                       formats.data());
  host.format = formats[0];
  for (auto& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      host.format = f;
      break;
    }
  }

  VkAttachmentDescription color{};
  color.format = host.format.format;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;
  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  rpci.attachmentCount = 1;
  rpci.pAttachments = &color;
  rpci.subpassCount = 1;
  rpci.pSubpasses = &subpass;
  rpci.dependencyCount = 1;
  rpci.pDependencies = &dep;
  VKCHECK(vkCreateRenderPass(host.device, &rpci, nullptr, &host.render_pass));

  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pci.queueFamilyIndex = host.qfam;
  VKCHECK(vkCreateCommandPool(host.device, &pci, nullptr, &host.cmd_pool));
  VkCommandBufferAllocateInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cbi.commandPool = host.cmd_pool;
  cbi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbi.commandBufferCount = kFramesInFlight;
  VKCHECK(vkAllocateCommandBuffers(host.device, &cbi, host.cmd));
  for (int i = 0; i < kFramesInFlight; ++i) {
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VKCHECK(vkCreateSemaphore(host.device, &sci, nullptr, &host.image_available[i]));
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VKCHECK(vkCreateFence(host.device, &fci, nullptr, &host.in_flight[i]));
  }

  VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16};
  VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  dpci.poolSizeCount = 1;
  dpci.pPoolSizes = &ps;
  dpci.maxSets = 16;
  VKCHECK(vkCreateDescriptorPool(host.device, &dpci, nullptr, &host.desc_pool));

  CreateSwapchain(win);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: embed_scene <shader_dir> <font.ttf> [frames]\n");
    return 2;
  }
  int frames = argc > 3 ? atoi(argv[3]) : 0;

  if (!glfwInit()) Die("glfwInit failed");
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow* win = glfwCreateWindow(640, 480, "embed_scene", nullptr, nullptr);
  if (!win) Die("window creation failed");
  HostInit(win);

  // ultragui's text engine in draw-data mode (no RHI).
  TextEngine te;
  te.Init(nullptr);
  FontHandle font = te.LoadFont(argv[2]);

  vk::InitInfo bi{};
  bi.instance = host.instance;
  bi.physical_device = host.phys;
  bi.device = host.device;
  bi.queue_family = host.qfam;
  bi.queue = host.queue;
  bi.descriptor_pool = host.desc_pool;
  bi.render_pass = host.render_pass;
  bi.frames_in_flight = kFramesInFlight;
  bi.shader_dir = argv[1];
  if (!vk::Init(bi)) Die("ugui::vk::Init failed");

  TextureBackend& tb = vk::texture_backend();
  u8 checker[8 * 8 * 4];
  for (u32 y = 0; y < 8; ++y) {
    for (u32 x = 0; x < 8; ++x) {
      u8* p = checker + (y * 8 + x) * 4;
      bool on = ((x + y) & 1) != 0;
      p[0] = on ? 240 : 40;
      p[1] = on ? 200 : 60;
      p[2] = on ? 80 : 160;
      p[3] = 255;
    }
  }
  TextureId checker_tex =
      tb.CreateTexture(8, 8, RHIFormat::kRgba8Unorm, checker, RHIFilter::kLinear);
  u8 bgra[2 * 2 * 4] = {255, 0,   0,   255, 0,   255, 0,   255,
                        0,   0,   255, 255, 255, 255, 255, 128};
  TextureId bgra_tex =
      tb.CreateTexture(2, 2, RHIFormat::kBgra8Unorm, bgra, RHIFilter::kNearest);
  TextureId doomed =
      tb.CreateTexture(2, 2, RHIFormat::kBgra8Unorm, bgra, RHIFilter::kNearest);
  tb.DestroyTexture(doomed);  // its id now falls back to white

  u32 font_rev = ~0u;
  const Color none{0, 0, 0, 0};
  const Rect full{0, 0, 640, 480};
  const Rect clip{200, 280, 180, 120};
  for (int frame = 0; frames == 0 || frame < frames; ++frame) {
    glfwPollEvents();
    if (glfwWindowShouldClose(win)) break;

    te.BeginFrame();
    TextRun hello = te.Shape(font, "Hello, ultragui!", 16, 28.0f);
    TextRun clipped = te.Shape(font, "Scissored text is clipped here", 30, 22.0f);
    if (te.atlas_revision() != font_rev) {
      Vec2 as = te.atlas_size();
      vk::UpdateFontAtlas(te.atlas_pixels(), static_cast<u32>(as.x),
                          static_cast<u32>(as.y));
      font_rev = te.atlas_revision();
    }
    tb.UpdateTexture(checker_tex, checker);

    Mesh q, t;
    Solid(q, {20, 20, 180, 100}, {0.23f, 0.51f, 0.96f, 1},
          Vertex2D::PackRadii(12));                                    // 0
    Quad(q, {220, 20, 180, 100}, {0.9f, 0.3f, 0.3f, 1},
         {0.9f, 0.3f, 0.3f, 1}, Vertex2D::PackRadii(0, 20, 40, 8), 0.0f,
         3.0f, {1, 1, 1, 0.9f});                                       // 1
    Quad(q, {410, 16, 220, 120}, {0, 0, 0, 0.6f}, {0, 0, 0, 0.6f},
         Vertex2D::PackRadii(16), 10.0f, 0.0f, none);                  // 2
    Solid(q, {420, 20, 200, 100}, {0.95f, 0.95f, 0.97f, 1},
          Vertex2D::PackRadii(16));                                    // 3
    Quad(q, {20, 140, 280, 80}, {1, 0.6f, 0.1f, 1}, {0.6f, 0.1f, 0.9f, 1},
         Vertex2D::PackRadii(10), 0.0f, 0.0f, none);                   // 4
    Quad(q, {20, 240, 120, 60}, {0.2f, 0.8f, 0.5f, 1},
         {0.2f, 0.8f, 0.5f, 1}, Vertex2D::PackRadii(12), -4.0f, 0.0f,
         none);                                                        // 5
    Solid(q, {320, 140, 128, 128}, {1, 1, 1, 1}, Vertex2D::PackRadii(20));  // 6
    Solid(q, {470, 140, 150, 150}, {1, 1, 1, 1}, 0);                   // 7 atlas
    Solid(q, {20, 320, 64, 64}, {1, 1, 1, 1}, 0);                      // 8
    Quad(q, {160, 240, 300, 200}, {0.3f, 0.3f, 0.6f, 1},
         {0.3f, 0.3f, 0.6f, 1}, Vertex2D::PackRadii(30), 0.0f, 4.0f,
         {1, 0.8f, 0.2f, 1});                                          // 9
    Solid(q, {520, 320, 100, 100}, {0.5f, 0.9f, 0.9f, 1},
          Vertex2D::PackRadii(8));                                     // 10 doomed id
    Solid(q, {500, 300, 40, 40}, {1, 0, 0, 1}, 0);                     // 11 blur, skipped
    Text(t, {20, 420}, hello, {1, 1, 1, 1});
    u32 hello_indices = static_cast<u32>(t.i.size());
    Text(t, {170, 300}, clipped, {1, 0.95f, 0.4f, 1});

    std::vector<DrawCmd> cmds;
    cmds.push_back(Cmd(full, kNullTextureId, 0, 36, false));
    cmds.push_back(Cmd(full, checker_tex, 36, 6, false));
    cmds.push_back(Cmd(full, kFontTextureId, 42, 6, false));
    cmds.push_back(Cmd(full, bgra_tex, 48, 6, false));
    cmds.push_back(Cmd(full, 0, 0, hello_indices, true));
    cmds.push_back(Cmd(clip, kNullTextureId, 54, 6, false));
    cmds.push_back(Cmd(clip, 0, hello_indices,
                       static_cast<u32>(t.i.size()) - hello_indices, true));
    cmds.push_back(Cmd(full, doomed, 60, 6, false));
    cmds.push_back(Cmd(full, kNullTextureId, 66, 6, false, 8.0f));
    cmds.push_back(Cmd({-50, -50, 0, 0}, kNullTextureId, 0, 0, false));

    DrawData dd;
    dd.quad_vertices = q.v.data();
    dd.quad_vertex_count = static_cast<u32>(q.v.size());
    dd.quad_indices = q.i.data();
    dd.quad_index_count = static_cast<u32>(q.i.size());
    dd.text_vertices = t.v.data();
    dd.text_vertex_count = static_cast<u32>(t.v.size());
    dd.text_indices = t.i.data();
    dd.text_index_count = static_cast<u32>(t.i.size());
    dd.commands = cmds.data();
    dd.command_count = static_cast<u32>(cmds.size());
    dd.display_size = {640, 480};
    dd.framebuffer_scale = {1, 1};
    dd.valid = true;

    vk::NewFrame();
    uint32_t fi = host.frame;
    vkWaitForFences(host.device, 1, &host.in_flight[fi], VK_TRUE, UINT64_MAX);
    uint32_t image_index = 0;
    VkResult acq = vkAcquireNextImageKHR(host.device, host.swapchain, UINT64_MAX,
                                         host.image_available[fi], VK_NULL_HANDLE,
                                         &image_index);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) continue;
    vkResetFences(host.device, 1, &host.in_flight[fi]);

    VkCommandBuffer cmd = host.cmd[fi];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &cbbi);

    VkClearValue clear{};
    clear.color = {{0.08f, 0.09f, 0.12f, 1.0f}};
    VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = host.render_pass;
    rbi.framebuffer = host.framebuffers[image_index];
    rbi.renderArea = {{0, 0}, host.extent};
    rbi.clearValueCount = 1;
    rbi.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
    vk::RenderDrawData(dd, cmd);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &host.image_available[fi];
    si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &host.render_finished[image_index];
    VKCHECK(vkQueueSubmit(host.queue, 1, &si, host.in_flight[fi]));

    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &host.render_finished[image_index];
    present.swapchainCount = 1;
    present.pSwapchains = &host.swapchain;
    present.pImageIndices = &image_index;
    vkQueuePresentKHR(host.queue, &present);

    host.frame = (host.frame + 1) % kFramesInFlight;
  }

  vkDeviceWaitIdle(host.device);
  vk::Shutdown();
  te.Shutdown();
  DestroySwapchain();
  vkDestroyDescriptorPool(host.device, host.desc_pool, nullptr);
  for (int i = 0; i < kFramesInFlight; ++i) {
    vkDestroySemaphore(host.device, host.image_available[i], nullptr);
    vkDestroyFence(host.device, host.in_flight[i], nullptr);
  }
  vkDestroyCommandPool(host.device, host.cmd_pool, nullptr);
  vkDestroyRenderPass(host.device, host.render_pass, nullptr);
  vkDestroyDevice(host.device, nullptr);
  vkDestroySurfaceKHR(host.instance, host.surface, nullptr);
  vkDestroyInstance(host.instance, nullptr);
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
