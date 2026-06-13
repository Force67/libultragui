// embed_vulkan: drop ultragui on top of an application that owns its own Vulkan
// device, swapchain, render pass, command buffers and presentation, Dear ImGui
// style (cf. imgui's example_glfw_vulkan + imgui_impl_vulkan).
//
// Per frame the host: produces the UI draw list (ui.RenderDrawData), uploads
// the glyph atlas if it grew, acquires a swapchain image, begins ITS OWN render
// pass (clearing to an animated colour as a stand-in for the host's scene),
// records the UI via ugui::vk::RenderDrawData into ITS OWN command buffer, ends
// the pass, submits and presents. ultragui creates no graphics device.
//
// Build:
//   cmake -B build -G Ninja && cmake --build build --target
//   ultragui_embed_vulkan
//   ./build/examples/ultragui_embed_vulkan

#define GLFW_INCLUDE_VULKAN
#include <ugui/backends/ugui_impl_vulkan.h>
#include <ugui/svg/svg.h>
#include <ugui/ultragui.h>
#include <ugui/widgets/image.h>

#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using ugui::u32;

static const int kFramesInFlight = 2;

static const char* kUi = R"(
panel root {
  width: 100vw; height: 100vh;
  layout: column; justify: center; align: center;
  panel card {
    layout: column; gap: 10; padding: 28; align: center;
    background: #14142899; corner-radius: 18;
    border-color: #ffffff20; border-width: 1;
    shadow-color: #00000080; shadow-blur: 28; shadow-y: 10;
    image logo { width: 72; height: 72; }
    text title { text: "ultragui via ugui_impl_vulkan"; font-size: 22; color: #ffffff; }
    text sub {
      text: "Host owns the Vulkan device, swapchain and render pass.";
      font-size: 13; color: #a8a8c8;
    }
    button btn_hello {
      text: "Click me"; background: #4a4aff; color: #ffffff;
      corner-radius: 8; padding: 10 20; cursor: pointer;
    }
  }
}
)";

// A tiny SVG, rasterized on the CPU and uploaded through the host backend to
// prove Image/SVG textures work in draw-data mode (host owns the device).
static const char* kLogoSvg = R"(
<svg viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">
  <circle cx="50" cy="50" r="46" fill="#4a4aff"/>
  <circle cx="50" cy="50" r="46" fill="none" stroke="#ffffff" stroke-width="4"/>
  <rect x="30" y="30" width="40" height="40" rx="8" fill="#ffffff"/>
</svg>
)";

static const char* FindFont() {
  namespace fs = std::filesystem;
  // Ask fontconfig first (works on most Linux setups, incl. Nix).
  static std::string resolved;
  if (FILE* p = popen("fc-match -f '%{file}' sans 2>/dev/null", "r")) {
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, p);
    pclose(p);
    if (n > 0) {
      buf[n] = '\0';
      resolved = buf;
      if (fs::exists(resolved)) return resolved.c_str();
    }
  }
  static const char* candidates[] = {
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
      "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
      "/System/Library/Fonts/Helvetica.ttc",
  };
  for (auto* c : candidates)
    if (fs::exists(c)) return c;
  return nullptr;
}

static void Die(const char* msg) {
  std::fprintf(stderr, "embed_vulkan: %s\n", msg);
  std::exit(1);
}
#define VKCHECK(x)                  \
  do {                              \
    if ((x) != VK_SUCCESS) Die(#x); \
  } while (0)

// ---------------------------------------------------------------------------
// Host Vulkan state
// ---------------------------------------------------------------------------
struct Vk {
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
  std::vector<VkSemaphore> render_finished;  // per swapchain image

  VkCommandBuffer cmd[kFramesInFlight]{};
  VkSemaphore image_available[kFramesInFlight]{};
  VkFence in_flight[kFramesInFlight]{};
  uint32_t frame = 0;
};
static Vk vk;

static void CreateSwapchain(GLFWwindow* win) {
  VkSurfaceCapabilitiesKHR caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.phys, vk.surface, &caps);

  vk.extent = caps.currentExtent;
  if (vk.extent.width == 0xFFFFFFFFu) {
    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    vk.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
  }

  uint32_t img_count = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && img_count > caps.maxImageCount)
    img_count = caps.maxImageCount;

  VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  ci.surface = vk.surface;
  ci.minImageCount = img_count;
  ci.imageFormat = vk.format.format;
  ci.imageColorSpace = vk.format.colorSpace;
  ci.imageExtent = vk.extent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.preTransform = caps.currentTransform;
  ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  ci.clipped = VK_TRUE;
  VKCHECK(vkCreateSwapchainKHR(vk.device, &ci, nullptr, &vk.swapchain));

  uint32_t n = 0;
  vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &n, nullptr);
  std::vector<VkImage> images(n);
  vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &n, images.data());

  vk.views.resize(n);
  vk.framebuffers.resize(n);
  vk.render_finished.resize(n);
  for (uint32_t i = 0; i < n; ++i) {
    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = images[i];
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = vk.format.format;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VKCHECK(vkCreateImageView(vk.device, &vci, nullptr, &vk.views[i]));

    VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = vk.render_pass;
    fci.attachmentCount = 1;
    fci.pAttachments = &vk.views[i];
    fci.width = vk.extent.width;
    fci.height = vk.extent.height;
    fci.layers = 1;
    VKCHECK(vkCreateFramebuffer(vk.device, &fci, nullptr, &vk.framebuffers[i]));

    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VKCHECK(
        vkCreateSemaphore(vk.device, &sci, nullptr, &vk.render_finished[i]));
  }
}

static void DestroySwapchain() {
  for (auto fb : vk.framebuffers) vkDestroyFramebuffer(vk.device, fb, nullptr);
  for (auto v : vk.views) vkDestroyImageView(vk.device, v, nullptr);
  for (auto s : vk.render_finished) vkDestroySemaphore(vk.device, s, nullptr);
  vk.framebuffers.clear();
  vk.views.clear();
  vk.render_finished.clear();
  if (vk.swapchain) vkDestroySwapchainKHR(vk.device, vk.swapchain, nullptr);
  vk.swapchain = VK_NULL_HANDLE;
}

static void RecreateSwapchain(GLFWwindow* win) {
  int w = 0, h = 0;
  glfwGetFramebufferSize(win, &w, &h);
  while (w == 0 || h == 0) {
    glfwWaitEvents();
    glfwGetFramebufferSize(win, &w, &h);
  }
  vkDeviceWaitIdle(vk.device);
  DestroySwapchain();
  CreateSwapchain(win);
}

int main() {
  if (!glfwInit()) Die("glfwInit failed");
  if (!glfwVulkanSupported()) Die("Vulkan not supported by GLFW");
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* win = glfwCreateWindow(
      1280, 800, "ultragui embedded (host owns Vulkan)", nullptr, nullptr);
  if (!win) Die("window creation failed");

  // --- Instance ---
  uint32_t ext_count = 0;
  const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "embed_vulkan";
  app.apiVersion = VK_API_VERSION_1_1;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo = &app;
  ici.enabledExtensionCount = ext_count;
  ici.ppEnabledExtensionNames = exts;
  VKCHECK(vkCreateInstance(&ici, nullptr, &vk.instance));

  VKCHECK(glfwCreateWindowSurface(vk.instance, win, nullptr, &vk.surface));

  // --- Physical device + queue family (graphics + present) ---
  uint32_t dev_count = 0;
  vkEnumeratePhysicalDevices(vk.instance, &dev_count, nullptr);
  std::vector<VkPhysicalDevice> devs(dev_count);
  vkEnumeratePhysicalDevices(vk.instance, &dev_count, devs.data());
  for (auto d : devs) {
    uint32_t qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(d, &qn, nullptr);
    std::vector<VkQueueFamilyProperties> qprops(qn);
    vkGetPhysicalDeviceQueueFamilyProperties(d, &qn, qprops.data());
    for (uint32_t i = 0; i < qn; ++i) {
      VkBool32 present = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(d, i, vk.surface, &present);
      if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
        vk.phys = d;
        vk.qfam = i;
        break;
      }
    }
    if (vk.phys) break;
  }
  if (!vk.phys) Die("no suitable GPU");

  // --- Logical device + queue ---
  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = vk.qfam;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;
  const char* dev_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.enabledExtensionCount = 1;
  dci.ppEnabledExtensionNames = dev_exts;
  VKCHECK(vkCreateDevice(vk.phys, &dci, nullptr, &vk.device));
  vkGetDeviceQueue(vk.device, vk.qfam, 0, &vk.queue);

  // --- Surface format (prefer sRGB to match ultragui's colour pipeline) ---
  uint32_t fmt_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(vk.phys, vk.surface, &fmt_count,
                                       nullptr);
  std::vector<VkSurfaceFormatKHR> formats(fmt_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(vk.phys, vk.surface, &fmt_count,
                                       formats.data());
  vk.format = formats[0];
  for (auto& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      vk.format = f;
      break;
    }
  }

  // --- Render pass ---
  VkAttachmentDescription color{};
  color.format = vk.format.format;
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
  VKCHECK(vkCreateRenderPass(vk.device, &rpci, nullptr, &vk.render_pass));

  // --- Command pool + per-frame command buffers + sync ---
  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pci.queueFamilyIndex = vk.qfam;
  VKCHECK(vkCreateCommandPool(vk.device, &pci, nullptr, &vk.cmd_pool));
  VkCommandBufferAllocateInfo cbi{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cbi.commandPool = vk.cmd_pool;
  cbi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbi.commandBufferCount = kFramesInFlight;
  VKCHECK(vkAllocateCommandBuffers(vk.device, &cbi, vk.cmd));
  for (int i = 0; i < kFramesInFlight; ++i) {
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VKCHECK(
        vkCreateSemaphore(vk.device, &sci, nullptr, &vk.image_available[i]));
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VKCHECK(vkCreateFence(vk.device, &fci, nullptr, &vk.in_flight[i]));
  }

  // --- Descriptor pool for the ultragui backend ---
  VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16};
  VkDescriptorPoolCreateInfo dpci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  dpci.poolSizeCount = 1;
  dpci.pPoolSizes = &ps;
  dpci.maxSets = 16;
  VKCHECK(vkCreateDescriptorPool(vk.device, &dpci, nullptr, &vk.desc_pool));

  CreateSwapchain(win);

  // --- ultragui in draw-data mode (no graphics device of its own) ---
  ugui::UIContext ui;
  ugui::UIConfig cfg;
  cfg.external_window = win;
  cfg.draw_data = true;
  cfg.width = 1280;
  cfg.height = 800;
  if (!ui.Init(cfg)) Die("ui.Init failed");
  if (const char* font = FindFont())
    ui.set_default_font(ui.LoadFont(font));
  else
    std::fprintf(stderr, "embed_vulkan: no system font found\n");
  ui.LoadUiString(kUi, "embed");

  int clicks = 0;
  ui.input().set_on_click([&](ugui::wid w, ugui::MouseButton) {
    ugui::WidgetNode* n = ui.world().Get<ugui::WidgetNode>(w);
    if (n && n->name == "btn_hello")
      std::printf("embed_vulkan: button clicked (%d)\n", ++clicks);
  });

  // --- ultragui Vulkan backend (records into the host's command buffer) ---
  ugui::vk::InitInfo bi{};
  bi.instance = vk.instance;
  bi.physical_device = vk.phys;
  bi.device = vk.device;
  bi.queue_family = vk.qfam;
  bi.queue = vk.queue;
  bi.descriptor_pool = vk.desc_pool;
  bi.render_pass = vk.render_pass;
  bi.frames_in_flight = kFramesInFlight;
  bi.shader_dir = ULTRAGUI_SHADER_DIR;
  if (!ugui::vk::Init(bi)) Die("ugui::vk::Init failed");

  // Hand ultragui the host's texture sink, then rasterize an SVG and attach it
  // to the Image widget. None of this was possible in draw-data mode before the
  // TextureBackend seam: the library has no device, so it routes the upload to
  // the host backend (ugui::vk) and references the result by its TextureId.
  ui.set_texture_backend(&ugui::vk::texture_backend());
  {
    ugui::SvgImage img;
    if (ugui::LoadSvgMemory(kLogoSvg, std::strlen(kLogoSvg), img, 72, 72)) {
      ugui::TextureId logo = ui.texture_backend()->CreateTexture(
          img.width, img.height, ugui::RHIFormat::kRgba8Unorm,
          img.pixels.data());
      ugui::SetImageTexture(ui.FindWidget("logo"), logo,
                            static_cast<float>(img.width),
                            static_cast<float>(img.height));
    }
  }

  u32 font_rev = ~0u;

  while (!glfwWindowShouldClose(win)) {
    // 1) Produce the UI draw list (polls input via the shared window).
    const ugui::DrawData& dd = ui.RenderDrawData();

    // 2) Upload the glyph atlas if new glyphs were rasterized this frame.
    if (ui.text_engine().atlas_revision() != font_rev) {
      ugui::Vec2 as = ui.text_engine().atlas_size();
      ugui::vk::UpdateFontAtlas(ui.text_engine().atlas_pixels(),
                                static_cast<u32>(as.x), static_cast<u32>(as.y));
      font_rev = ui.text_engine().atlas_revision();
    }

    ugui::vk::NewFrame();
    uint32_t fi = vk.frame;
    vkWaitForFences(vk.device, 1, &vk.in_flight[fi], VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult acq = vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
                                         vk.image_available[fi], VK_NULL_HANDLE,
                                         &image_index);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
      RecreateSwapchain(win);
      continue;
    }
    vkResetFences(vk.device, 1, &vk.in_flight[fi]);

    VkCommandBuffer cmd = vk.cmd[fi];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &cbbi);

    // The HOST's own render pass. Clear is the host's "scene" stand-in.
    float t = static_cast<float>(glfwGetTime());
    VkClearValue clear{};
    clear.color = {
        {0.05f + 0.05f * (0.5f + 0.5f * std::sin(t)), 0.06f, 0.10f, 1.0f}};
    VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = vk.render_pass;
    rbi.framebuffer = vk.framebuffers[image_index];
    rbi.renderArea = {{0, 0}, vk.extent};
    rbi.clearValueCount = 1;
    rbi.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);

    // ultragui on top.
    ugui::vk::RenderDrawData(dd, cmd);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &vk.image_available[fi];
    si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &vk.render_finished[image_index];
    VKCHECK(vkQueueSubmit(vk.queue, 1, &si, vk.in_flight[fi]));

    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &vk.render_finished[image_index];
    present.swapchainCount = 1;
    present.pSwapchains = &vk.swapchain;
    present.pImageIndices = &image_index;
    VkResult pres = vkQueuePresentKHR(vk.queue, &present);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR)
      RecreateSwapchain(win);

    vk.frame = (vk.frame + 1) % kFramesInFlight;
  }

  vkDeviceWaitIdle(vk.device);
  ugui::vk::Shutdown();
  ui.Shutdown();

  DestroySwapchain();
  vkDestroyDescriptorPool(vk.device, vk.desc_pool, nullptr);
  for (int i = 0; i < kFramesInFlight; ++i) {
    vkDestroySemaphore(vk.device, vk.image_available[i], nullptr);
    vkDestroyFence(vk.device, vk.in_flight[i], nullptr);
  }
  vkDestroyCommandPool(vk.device, vk.cmd_pool, nullptr);
  vkDestroyRenderPass(vk.device, vk.render_pass, nullptr);
  vkDestroyDevice(vk.device, nullptr);
  vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
  vkDestroyInstance(vk.instance, nullptr);
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
