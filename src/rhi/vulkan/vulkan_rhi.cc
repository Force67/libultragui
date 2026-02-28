#include "vulkan_rhi.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <vector>

namespace ugui {

static RHI::Impl* s_rhi_instance = nullptr;

// ---------------------------------------------------------------------------
// Debug callback
// ---------------------------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*user*/) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::fprintf(stderr, "[vulkan] %s\n", data->pMessage);
    }
    return VK_FALSE;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Vector<char> read_file(const String& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        return {};
    auto size = static_cast<size_t>(file.tellg());
    Vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), static_cast<std::streamsize>(size));
    return buf;
}

static VkSurfaceFormatKHR choose_surface_format(const Vector<VkSurfaceFormatKHR>& formats) {
    // Prefer sRGB format: the GPU automatically converts linear framebuffer writes
    // to sRGB on output, giving correct gamma-space anti-aliasing and blending.
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    // Fallback to UNORM + sRGB colorspace (manual gamma)
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

static VkPresentModeKHR choose_present_mode(const Vector<VkPresentModeKHR>& modes,
                                            bool vsync) {
    if (vsync)
        return VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window) {
    if (caps.currentExtent.width != UINT32_MAX)
        return caps.currentExtent;
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    VkExtent2D ext = {static_cast<u32>(w), static_cast<u32>(h)};
    ext.width = std::clamp(ext.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    ext.height = std::clamp(ext.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return ext;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

bool RHI::Impl::Init(const RHIConfig& config) {
    window_ = static_cast<GLFWwindow*>(config.platform->native_handle());
    validation_enabled_ = config.validation;
    shader_dir_ = config.shader_dir ? config.shader_dir : "shaders";

    // Compute DPI scale: ratio of framebuffer pixels to window (screen) coordinates
    {
        int fw, fh, ww, wh;
        glfwGetFramebufferSize(window_, &fw, &fh);
        glfwGetWindowSize(window_, &ww, &wh);
        dpi_scale_ = (ww > 0) ? static_cast<f32>(fw) / static_cast<f32>(ww) : 1.0f;
    }

    // Use a static to avoid glfwSetWindowUserPointer conflict with InputRouter
    s_rhi_instance = this;
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow*, int, int) {
        if (!s_rhi_instance)
            return;
        s_rhi_instance->framebuffer_resized_ = true;
        int fw2, fh2, ww2, wh2;
        glfwGetFramebufferSize(s_rhi_instance->window_, &fw2, &fh2);
        glfwGetWindowSize(s_rhi_instance->window_, &ww2, &wh2);
        s_rhi_instance->dpi_scale_ =
            (ww2 > 0) ? static_cast<f32>(fw2) / static_cast<f32>(ww2) : 1.0f;
    });

    if (!create_instance())
        return false;
    if (!create_surface())
        return false;
    if (!pick_physical_device())
        return false;
    if (!create_device())
        return false;
    if (!create_swapchain())
        return false;
    if (!create_render_pass())
        return false;
    if (!create_descriptor_layout())
        return false;
    if (!create_pipeline())
        return false;
    if (!create_text_pipeline())
        return false;
    if (!create_framebuffers())
        return false;
    if (!create_command_resources())
        return false;
    if (!create_sync_objects())
        return false;
    if (!create_default_resources())
        return false;

    return true;
}

bool RHI::Impl::create_instance() {
    VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName = "ultragui";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "ultragui";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    u32 glfw_ext_count = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    Vector<const char*> extensions(glfw_exts, glfw_exts + glfw_ext_count);

    Vector<const char*> layers;
    if (validation_enabled_) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = static_cast<u32>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();
    ci.enabledLayerCount = static_cast<u32>(layers.size());
    ci.ppEnabledLayerNames = layers.data();

    if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS) {
        std::fprintf(stderr, "ultragui: vkCreateInstance failed\n");
        return false;
    }

    if (validation_enabled_) {
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        if (func) {
            VkDebugUtilsMessengerCreateInfoEXT dci{
                VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dci.pfnUserCallback = debug_callback;
            func(instance_, &dci, nullptr, &debug_messenger_);
        }
    }
    return true;
}

bool RHI::Impl::create_surface() {
    return glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) == VK_SUCCESS;
}

bool RHI::Impl::pick_physical_device() {
    u32 count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0)
        return false;
    Vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    for (auto dev : devices) {
        u32 qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, nullptr);
        Vector<VkQueueFamilyProperties> qfamilies(qcount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, qfamilies.data());

        bool found_graphics = false, found_present = false;
        for (u32 i = 0; i < qcount; ++i) {
            if (qfamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_family_ = i;
                found_graphics = true;
            }
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &present_support);
            if (present_support) {
                present_family_ = i;
                found_present = true;
            }
            if (found_graphics && found_present)
                break;
        }

        if (found_graphics && found_present) {
            physical_device_ = dev;
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            std::printf("ultragui: using GPU '%s'\n", props.deviceName);
            return true;
        }
    }
    return false;
}

bool RHI::Impl::create_device() {
    std::set<u32> unique_families = {graphics_family_, present_family_};
    float priority = 1.0f;
    Vector<VkDeviceQueueCreateInfo> queue_cis;
    for (u32 family : unique_families) {
        VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex = family;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        queue_cis.push_back(qci);
    }

    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.queueCreateInfoCount = static_cast<u32>(queue_cis.size());
    ci.pQueueCreateInfos = queue_cis.data();
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = device_extensions;
    ci.pEnabledFeatures = &features;

    if (vkCreateDevice(physical_device_, &ci, nullptr, &device_) != VK_SUCCESS)
        return false;

    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_family_, 0, &present_queue_);
    return true;
}

// ---------------------------------------------------------------------------
// Swapchain
// ---------------------------------------------------------------------------

bool RHI::Impl::create_swapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &caps);

    u32 fmt_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &fmt_count, nullptr);
    Vector<VkSurfaceFormatKHR> formats(fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &fmt_count, formats.data());

    u32 pm_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &pm_count, nullptr);
    Vector<VkPresentModeKHR> modes(pm_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &pm_count, modes.data());

    auto surface_format = choose_surface_format(formats);
    auto present_mode = choose_present_mode(modes, true);
    auto extent = choose_extent(caps, window_);

    u32 image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surface_;
    ci.minImageCount = image_count;
    ci.imageFormat = surface_format.format;
    ci.imageColorSpace = surface_format.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    u32 queue_indices[] = {graphics_family_, present_family_};
    if (graphics_family_ != present_family_) {
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

    if (vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_) != VK_SUCCESS)
        return false;

    swapchain_format_ = surface_format.format;
    swapchain_extent_ = extent;

    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
    swapchain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data());

    swapchain_views_.resize(image_count);
    for (u32 i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = swapchain_images_[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = swapchain_format_;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(device_, &vci, nullptr, &swapchain_views_[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

void RHI::Impl::cleanup_swapchain() {
    for (auto fb : framebuffers_)
        vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();
    for (auto view : swapchain_views_)
        vkDestroyImageView(device_, view, nullptr);
    swapchain_views_.clear();
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}

bool RHI::Impl::recreate_swapchain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window_, &w, &h);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(device_);
    cleanup_swapchain();
    if (!create_swapchain())
        return false;
    if (!create_framebuffers())
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// Render pass & pipeline
// ---------------------------------------------------------------------------

bool RHI::Impl::create_render_pass() {
    VkAttachmentDescription color_att{};
    color_att.format = swapchain_format_;
    color_att.samples = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 1;
    ci.pAttachments = &color_att;
    ci.subpassCount = 1;
    ci.pSubpasses = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;

    return vkCreateRenderPass(device_, &ci, nullptr, &render_pass_) == VK_SUCCESS;
}

bool RHI::Impl::create_descriptor_layout() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 1;
    ci.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(device_, &ci, nullptr, &desc_set_layout_) != VK_SUCCESS)
        return false;

    VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES};
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.poolSizeCount = 1;
    pci.pPoolSizes = &pool_size;
    pci.maxSets = MAX_TEXTURES;

    return vkCreateDescriptorPool(device_, &pci, nullptr, &desc_pool_) == VK_SUCCESS;
}

VkShaderModule RHI::Impl::load_shader(const char* filename) {
    auto code = read_file(shader_dir_ + "/" + filename);
    if (code.empty()) {
        std::fprintf(stderr, "ultragui: failed to load shader '%s/%s'\n", shader_dir_.c_str(),
                     filename);
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const u32*>(code.data());

    VkShaderModule mod;
    if (vkCreateShaderModule(device_, &ci, nullptr, &mod) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return mod;
}

bool RHI::Impl::create_pipeline() {
    auto vert_mod = load_shader("quad.vert.spv");
    auto frag_mod = load_shader("quad.frag.spv");
    if (!vert_mod || !frag_mod)
        return false;

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_mod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    // Vertex input: matches Vertex2D layout
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

    VkPipelineVertexInputStateCreateInfo vertex_input{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 9;
    vertex_input.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_att{};
    blend_att.blendEnable = VK_TRUE;
    blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_att.colorBlendOp = VK_BLEND_OP_ADD;
    blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
    blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &blend_att;

    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dyn_states;

    // Push constant: orthographic projection (scale + translate = 4 floats)
    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(f32) * 4;

    VkPipelineLayoutCreateInfo layout_ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_ci.setLayoutCount = 1;
    layout_ci.pSetLayouts = &desc_set_layout_;
    layout_ci.pushConstantRangeCount = 1;
    layout_ci.pPushConstantRanges = &push_range;

    if (vkCreatePipelineLayout(device_, &layout_ci, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vert_mod, nullptr);
        vkDestroyShaderModule(device_, frag_mod, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vertex_input;
    pi.pInputAssemblyState = &input_assembly;
    pi.pViewportState = &viewport_state;
    pi.pRasterizationState = &raster;
    pi.pMultisampleState = &multisample;
    pi.pColorBlendState = &color_blend;
    pi.pDynamicState = &dynamic_state;
    pi.layout = pipeline_layout_;
    pi.renderPass = render_pass_;
    pi.subpass = 0;

    VkResult result =
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &pipeline_);

    vkDestroyShaderModule(device_, vert_mod, nullptr);
    vkDestroyShaderModule(device_, frag_mod, nullptr);

    return result == VK_SUCCESS;
}

bool RHI::Impl::create_text_pipeline() {
    auto vert_mod = load_shader("text.vert.spv");
    auto frag_mod = load_shader("text.frag.spv");
    if (!vert_mod || !frag_mod)
        return false;

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_mod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    // Same vertex layout as quad pipeline
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

    VkPipelineVertexInputStateCreateInfo vertex_input{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 9;
    vertex_input.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    raster.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_att{};
    blend_att.blendEnable = VK_TRUE;
    blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_att.colorBlendOp = VK_BLEND_OP_ADD;
    blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
    blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &blend_att;

    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dyn_states;

    VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vertex_input;
    pi.pInputAssemblyState = &input_assembly;
    pi.pViewportState = &viewport_state;
    pi.pRasterizationState = &raster;
    pi.pMultisampleState = &multisample;
    pi.pColorBlendState = &color_blend;
    pi.pDynamicState = &dynamic_state;
    pi.layout = pipeline_layout_; // Same layout as quad pipeline
    pi.renderPass = render_pass_;
    pi.subpass = 0;

    VkResult result =
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &text_pipeline_);

    vkDestroyShaderModule(device_, vert_mod, nullptr);
    vkDestroyShaderModule(device_, frag_mod, nullptr);

    return result == VK_SUCCESS;
}

bool RHI::Impl::create_framebuffers() {
    framebuffers_.resize(swapchain_views_.size());
    for (size_t i = 0; i < swapchain_views_.size(); ++i) {
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        ci.renderPass = render_pass_;
        ci.attachmentCount = 1;
        ci.pAttachments = &swapchain_views_[i];
        ci.width = swapchain_extent_.width;
        ci.height = swapchain_extent_.height;
        ci.layers = 1;
        if (vkCreateFramebuffer(device_, &ci, nullptr, &framebuffers_[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Command & sync resources
// ---------------------------------------------------------------------------

bool RHI::Impl::create_command_resources() {
    for (u32 i = 0; i < MAX_FRAMES; ++i) {
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = graphics_family_;
        if (vkCreateCommandPool(device_, &pci, nullptr, &frames_[i].cmd_pool) != VK_SUCCESS)
            return false;

        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = frames_[i].cmd_pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device_, &ai, &frames_[i].cmd_buffer) != VK_SUCCESS)
            return false;
    }
    return true;
}

bool RHI::Impl::create_sync_objects() {
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (u32 i = 0; i < MAX_FRAMES; ++i) {
        if (vkCreateSemaphore(device_, &sci, nullptr, &frames_[i].image_available) != VK_SUCCESS)
            return false;
        if (vkCreateFence(device_, &fci, nullptr, &frames_[i].in_flight) != VK_SUCCESS)
            return false;
    }

    // Per-swapchain-image semaphores: safe to reuse only after image re-acquired
    u32 image_count = static_cast<u32>(swapchain_images_.size());
    for (u32 i = 0; i < image_count && i < MAX_SWAPCHAIN_IMAGES; ++i) {
        if (vkCreateSemaphore(device_, &sci, nullptr, &render_finished_[i]) != VK_SUCCESS)
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Buffer helpers
// ---------------------------------------------------------------------------

u32 RHI::Impl::find_memory_type(u32 type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props);
    for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    assert(false && "failed to find suitable memory type");
    return 0;
}

void RHI::Impl::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags properties, VkBuffer& buffer,
                              VkDeviceMemory& memory) {
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device_, &ci, nullptr, &buffer);

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device_, buffer, &reqs);

    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = reqs.size;
    ai.memoryTypeIndex = find_memory_type(reqs.memoryTypeBits, properties);
    vkAllocateMemory(device_, &ai, nullptr, &memory);
    vkBindBufferMemory(device_, buffer, memory, 0);
}

void RHI::Impl::ensure_vertex_capacity(u32 vertex_count) {
    auto& f = frames_[current_frame_];
    if (f.vertex_capacity >= vertex_count)
        return;

    if (f.vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, f.vertex_buffer, nullptr);
        vkFreeMemory(device_, f.vertex_memory, nullptr);
    }

    u32 new_cap = std::max(vertex_count, f.vertex_capacity * 2);
    new_cap = std::max(new_cap, 16384u);
    create_buffer(new_cap * sizeof(Vertex2D), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  f.vertex_buffer, f.vertex_memory);
    f.vertex_capacity = new_cap;
}

void RHI::Impl::ensure_index_capacity(u32 index_count) {
    auto& f = frames_[current_frame_];
    if (f.index_capacity >= index_count)
        return;

    if (f.index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, f.index_buffer, nullptr);
        vkFreeMemory(device_, f.index_memory, nullptr);
    }

    u32 new_cap = std::max(index_count, f.index_capacity * 2);
    new_cap = std::max(new_cap, 32768u);
    create_buffer(new_cap * sizeof(u32), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  f.index_buffer, f.index_memory);
    f.index_capacity = new_cap;
}

void RHI::Impl::ensure_text_vertex_capacity(u32 vertex_count) {
    auto& f = frames_[current_frame_];
    if (f.text_vertex_capacity >= vertex_count)
        return;

    if (f.text_vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, f.text_vertex_buffer, nullptr);
        vkFreeMemory(device_, f.text_vertex_memory, nullptr);
    }

    u32 new_cap = std::max(vertex_count, f.text_vertex_capacity * 2);
    new_cap = std::max(new_cap, 16384u);
    create_buffer(new_cap * sizeof(Vertex2D), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  f.text_vertex_buffer, f.text_vertex_memory);
    f.text_vertex_capacity = new_cap;
}

void RHI::Impl::ensure_text_index_capacity(u32 index_count) {
    auto& f = frames_[current_frame_];
    if (f.text_index_capacity >= index_count)
        return;

    if (f.text_index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, f.text_index_buffer, nullptr);
        vkFreeMemory(device_, f.text_index_memory, nullptr);
    }

    u32 new_cap = std::max(index_count, f.text_index_capacity * 2);
    new_cap = std::max(new_cap, 32768u);
    create_buffer(new_cap * sizeof(u32), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  f.text_index_buffer, f.text_index_memory);
    f.text_index_capacity = new_cap;
}

// ---------------------------------------------------------------------------
// Default resources (white texture + sampler)
// ---------------------------------------------------------------------------

bool RHI::Impl::create_default_resources() {
    // Linear sampler (images, general textures)
    VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(device_, &sci, nullptr, &default_sampler_) != VK_SUCCESS)
        return false;

    // Nearest sampler (text atlas: pixel-exact, no inter-glyph bleeding)
    VkSamplerCreateInfo nsci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    nsci.magFilter = VK_FILTER_NEAREST;
    nsci.minFilter = VK_FILTER_NEAREST;
    nsci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    nsci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    nsci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(device_, &nsci, nullptr, &nearest_sampler_) != VK_SUCCESS)
        return false;

    // White 1x1 texture
    u32 white_pixel = 0xFFFFFFFF;
    white_texture_ = CreateTexture(1, 1, RHIFormat::kRgba8Unorm, &white_pixel);
    return white_texture_ != kInvalidTexture;
}

// ---------------------------------------------------------------------------
// Texture management
// ---------------------------------------------------------------------------

RHITextureHandle RHI::Impl::CreateTexture(u32 width, u32 height, RHIFormat format,
                                           const void* pixels, RHIFilter filter) {
    // Find a free slot
    RHITextureHandle handle = kInvalidTexture;
    for (u32 i = 0; i < MAX_TEXTURES; ++i) {
        if (!textures_[i].in_use) {
            handle = i;
            break;
        }
    }
    if (handle == kInvalidTexture)
        return kInvalidTexture;

    auto& slot = textures_[handle];
    VkFormat vk_format = VK_FORMAT_R8G8B8A8_UNORM;
    u32 pixel_size = 4;
    switch (format) {
    case RHIFormat::kRgba8Unorm:
        vk_format = VK_FORMAT_R8G8B8A8_UNORM;
        pixel_size = 4;
        break;
    case RHIFormat::kBgra8Unorm:
        vk_format = VK_FORMAT_B8G8R8A8_UNORM;
        pixel_size = 4;
        break;
    case RHIFormat::kR8Unorm:
        vk_format = VK_FORMAT_R8_UNORM;
        pixel_size = 1;
        break;
    default:
        return kInvalidTexture;
    }

    VkDeviceSize image_size = width * height * pixel_size;

    // Staging buffer
    VkBuffer staging;
    VkDeviceMemory staging_mem;
    create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  staging, staging_mem);

    void* data;
    vkMapMemory(device_, staging_mem, 0, image_size, 0, &data);
    std::memcpy(data, pixels, image_size);
    vkUnmapMemory(device_, staging_mem);

    // Image
    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent = {width, height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = vk_format;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    vkCreateImage(device_, &ici, nullptr, &slot.image);

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(device_, slot.image, &reqs);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = reqs.size;
    ai.memoryTypeIndex = find_memory_type(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &ai, nullptr, &slot.memory);
    vkBindImageMemory(device_, slot.image, slot.memory, 0);

    // Copy staging -> image (using a temporary command buffer)
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = frames_[0].cmd_pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    vkAllocateCommandBuffers(device_, &cai, &cmd);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    // Transition: UNDEFINED -> TRANSFER_DST
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = slot.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, staging, slot.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);

    // Transition: TRANSFER_DST -> SHADER_READ
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(graphics_queue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);
    vkFreeCommandBuffers(device_, frames_[0].cmd_pool, 1, &cmd);

    vkDestroyBuffer(device_, staging, nullptr);
    vkFreeMemory(device_, staging_mem, nullptr);

    // Image view
    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = slot.image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = vk_format;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(device_, &vci, nullptr, &slot.view);

    // Descriptor set
    VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = desc_pool_;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &desc_set_layout_;
    vkAllocateDescriptorSets(device_, &dai, &slot.descriptor);

    VkDescriptorImageInfo dii{};
    dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dii.imageView = slot.view;
    dii.sampler = (filter == RHIFilter::kNearest) ? nearest_sampler_ : default_sampler_;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = slot.descriptor;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &dii;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    slot.width = width;
    slot.height = height;
    slot.pixel_size = pixel_size;
    slot.in_use = true;
    return handle;
}

void RHI::Impl::UpdateTexture(RHITextureHandle handle, const void* pixels) {
    if (handle >= MAX_TEXTURES || !textures_[handle].in_use)
        return;

    auto& slot = textures_[handle];
    VkDeviceSize image_size = slot.width * slot.height * slot.pixel_size;

    // Staging buffer
    VkBuffer staging;
    VkDeviceMemory staging_mem;
    create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  staging, staging_mem);

    void* data;
    vkMapMemory(device_, staging_mem, 0, image_size, 0, &data);
    std::memcpy(data, pixels, image_size);
    vkUnmapMemory(device_, staging_mem);

    // Upload via temporary command buffer
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = frames_[0].cmd_pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    vkAllocateCommandBuffers(device_, &cai, &cmd);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    // Transition: SHADER_READ -> TRANSFER_DST
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = slot.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {slot.width, slot.height, 1};
    vkCmdCopyBufferToImage(cmd, staging, slot.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);

    // Transition: TRANSFER_DST -> SHADER_READ
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(graphics_queue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);
    vkFreeCommandBuffers(device_, frames_[0].cmd_pool, 1, &cmd);

    vkDestroyBuffer(device_, staging, nullptr);
    vkFreeMemory(device_, staging_mem, nullptr);
}

void RHI::Impl::DestroyTexture(RHITextureHandle handle) {
    if (handle >= MAX_TEXTURES || !textures_[handle].in_use)
        return;
    if (textures_[handle].is_render_target) {
        DestroyRenderTarget(handle);
        return;
    }
    vkDeviceWaitIdle(device_);
    auto& slot = textures_[handle];
    vkDestroyImageView(device_, slot.view, nullptr);
    vkDestroyImage(device_, slot.image, nullptr);
    vkFreeMemory(device_, slot.memory, nullptr);
    slot = {};
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------

bool RHI::Impl::AcquireFrame() {
    if (frame_acquired_)
        return true;

    auto& f = frames_[current_frame_];
    vkWaitForFences(device_, 1, &f.in_flight, VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, f.image_available,
                                            VK_NULL_HANDLE, &image_index_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return false;
    }

    vkResetFences(device_, 1, &f.in_flight);
    vkResetCommandBuffer(f.cmd_buffer, 0);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f.cmd_buffer, &bi);

    // Reset write positions for the ring-buffer approach
    f.vertex_write_pos = 0;
    f.index_write_pos = 0;
    f.text_vertex_write_pos = 0;
    f.text_index_write_pos = 0;

    // Reset vertex dedup tracking
    last_quad_verts_ = nullptr;
    last_quad_vert_count_ = 0;
    last_text_verts_ = nullptr;
    last_text_vert_count_ = 0;

    frame_acquired_ = true;
    return true;
}

bool RHI::Impl::BeginFrame(Color clear_color) {
    if (!AcquireFrame())
        return false;

    auto& f = frames_[current_frame_];

    VkClearValue clear = {};
    clear.color = {{clear_color.r, clear_color.g, clear_color.b, clear_color.a}};

    VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = render_pass_;
    rbi.framebuffer = framebuffers_[image_index_];
    rbi.renderArea = {{0, 0}, swapchain_extent_};
    rbi.clearValueCount = 1;
    rbi.pClearValues = &clear;
    vkCmdBeginRenderPass(f.cmd_buffer, &rbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(f.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{};
    viewport.width = static_cast<f32>(swapchain_extent_.width);
    viewport.height = static_cast<f32>(swapchain_extent_.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(f.cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, swapchain_extent_};
    vkCmdSetScissor(f.cmd_buffer, 0, 1, &scissor);

    // Push orthographic projection: use window (screen) coordinates so that
    // vertex positions match the mouse/input coordinate space.
    f32 win_w = static_cast<f32>(swapchain_extent_.width) / dpi_scale_;
    f32 win_h = static_cast<f32>(swapchain_extent_.height) / dpi_scale_;
    f32 push[4] = {
        2.0f / win_w,
        2.0f / win_h,
        -1.0f,
        -1.0f,
    };
    vkCmdPushConstants(f.cmd_buffer, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push),
                       push);

    return true;
}

void RHI::Impl::EndFrame() {
    auto& f = frames_[current_frame_];
    vkCmdEndRenderPass(f.cmd_buffer);
    vkEndCommandBuffer(f.cmd_buffer);

    VkSemaphore signal_sem = render_finished_[image_index_];

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &f.image_available;
    si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &f.cmd_buffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &signal_sem;
    vkQueueSubmit(graphics_queue_, 1, &si, f.in_flight);

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &signal_sem;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_;
    pi.pImageIndices = &image_index_;

    VkResult result = vkQueuePresentKHR(present_queue_, &pi);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized_) {
        framebuffer_resized_ = false;
        recreate_swapchain();
    }

    frame_acquired_ = false;
    current_frame_ = (current_frame_ + 1) % MAX_FRAMES;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void RHI::Impl::SetScissor(Rect rect) {
    auto& f = frames_[current_frame_];
    // Offscreen targets are pixel-exact (no DPI scaling)
    f32 scale = (active_offscreen_target_ != kInvalidTexture) ? 1.0f : dpi_scale_;
    VkRect2D scissor{};
    scissor.offset = {static_cast<i32>(rect.x * scale),
                      static_cast<i32>(rect.y * scale)};
    scissor.extent = {static_cast<u32>(rect.w * scale),
                      static_cast<u32>(rect.h * scale)};
    vkCmdSetScissor(f.cmd_buffer, 0, 1, &scissor);
}

void RHI::Impl::ResetScissor() {
    auto& f = frames_[current_frame_];
    if (active_offscreen_target_ != kInvalidTexture) {
        auto& slot = textures_[active_offscreen_target_];
        VkRect2D scissor{{0, 0}, {slot.width, slot.height}};
        vkCmdSetScissor(f.cmd_buffer, 0, 1, &scissor);
    } else {
        VkRect2D scissor{{0, 0}, swapchain_extent_};
        vkCmdSetScissor(f.cmd_buffer, 0, 1, &scissor);
    }
}

void RHI::Impl::DrawTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                               u32 index_count, RHITextureHandle texture) {
    if (vertex_count == 0 || index_count == 0)
        return;

    auto& f = frames_[current_frame_];

    // Renderer2D passes the SAME vertex array for every batch in a frame,
    // with different index slices. Detect this to avoid redundant uploads.
    VkDeviceSize vb_byte_offset;
    if (vertices == last_quad_verts_ && vertex_count == last_quad_vert_count_) {
        vb_byte_offset = last_quad_vb_offset_;
    } else {
        ensure_vertex_capacity(f.vertex_write_pos + vertex_count);
        vb_byte_offset = f.vertex_write_pos * sizeof(Vertex2D);
        void* data;
        vkMapMemory(device_, f.vertex_memory, vb_byte_offset, vertex_count * sizeof(Vertex2D), 0,
                    &data);
        std::memcpy(data, vertices, vertex_count * sizeof(Vertex2D));
        vkUnmapMemory(device_, f.vertex_memory);
        last_quad_verts_ = vertices;
        last_quad_vert_count_ = vertex_count;
        last_quad_vb_offset_ = vb_byte_offset;
        f.vertex_write_pos += vertex_count;
    }

    // Indices are always different per batch: always append
    ensure_index_capacity(f.index_write_pos + index_count);
    VkDeviceSize ib_byte_offset = f.index_write_pos * sizeof(u32);
    void* data;
    vkMapMemory(device_, f.index_memory, ib_byte_offset, index_count * sizeof(u32), 0, &data);
    std::memcpy(data, indices, index_count * sizeof(u32));
    vkUnmapMemory(device_, f.index_memory);
    f.index_write_pos += index_count;

    // Bind texture descriptor
    RHITextureHandle tex = (texture != kInvalidTexture) ? texture : white_texture_;
    if (tex < MAX_TEXTURES && textures_[tex].in_use) {
        vkCmdBindDescriptorSets(f.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0,
                                1, &textures_[tex].descriptor, 0, nullptr);
    }

    // Draw
    vkCmdBindVertexBuffers(f.cmd_buffer, 0, 1, &f.vertex_buffer, &vb_byte_offset);
    vkCmdBindIndexBuffer(f.cmd_buffer, f.index_buffer, ib_byte_offset, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(f.cmd_buffer, index_count, 1, 0, 0, 0);
}

void RHI::Impl::DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                                    u32 index_count, RHITextureHandle atlas_texture) {
    if (vertex_count == 0 || index_count == 0)
        return;

    auto& f = frames_[current_frame_];

    // Use SEPARATE buffers for text to avoid overwriting quad data.
    // Deduplicate vertex uploads (same pattern as draw_triangles).
    VkDeviceSize vb_byte_offset;
    if (vertices == last_text_verts_ && vertex_count == last_text_vert_count_) {
        vb_byte_offset = last_text_vb_offset_;
    } else {
        ensure_text_vertex_capacity(f.text_vertex_write_pos + vertex_count);
        vb_byte_offset = f.text_vertex_write_pos * sizeof(Vertex2D);
        void* data;
        vkMapMemory(device_, f.text_vertex_memory, vb_byte_offset,
                    vertex_count * sizeof(Vertex2D), 0, &data);
        std::memcpy(data, vertices, vertex_count * sizeof(Vertex2D));
        vkUnmapMemory(device_, f.text_vertex_memory);
        last_text_verts_ = vertices;
        last_text_vert_count_ = vertex_count;
        last_text_vb_offset_ = vb_byte_offset;
        f.text_vertex_write_pos += vertex_count;
    }

    ensure_text_index_capacity(f.text_index_write_pos + index_count);
    VkDeviceSize ib_byte_offset = f.text_index_write_pos * sizeof(u32);
    void* data;
    vkMapMemory(device_, f.text_index_memory, ib_byte_offset, index_count * sizeof(u32), 0, &data);
    std::memcpy(data, indices, index_count * sizeof(u32));
    vkUnmapMemory(device_, f.text_index_memory);
    f.text_index_write_pos += index_count;

    // Switch to text pipeline
    vkCmdBindPipeline(f.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, text_pipeline_);

    // Bind atlas texture
    RHITextureHandle tex = atlas_texture;
    if (tex < MAX_TEXTURES && textures_[tex].in_use) {
        vkCmdBindDescriptorSets(f.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0,
                                1, &textures_[tex].descriptor, 0, nullptr);
    }

    // Re-push constants (pipeline change resets them): use display_size()
    // which returns offscreen dimensions when inside an offscreen pass.
    Vec2 ds = display_size();
    f32 push[4] = {
        2.0f / ds.x,
        2.0f / ds.y,
        -1.0f,
        -1.0f,
    };
    vkCmdPushConstants(f.cmd_buffer, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push),
                       push);

    vkCmdBindVertexBuffers(f.cmd_buffer, 0, 1, &f.text_vertex_buffer, &vb_byte_offset);
    vkCmdBindIndexBuffer(f.cmd_buffer, f.text_index_buffer, ib_byte_offset, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(f.cmd_buffer, index_count, 1, 0, 0, 0);

    f.text_vertex_write_pos += vertex_count;
    f.text_index_write_pos += index_count;

    // Switch back to quad pipeline
    vkCmdBindPipeline(f.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdPushConstants(f.cmd_buffer, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push),
                       push);
}

Vec2 RHI::Impl::display_size() const {
    if (active_offscreen_target_ != kInvalidTexture)
        return offscreen_display_size_;
    // Return window/screen coordinates (UI coordinate space), not framebuffer pixels
    return {static_cast<f32>(swapchain_extent_.width) / dpi_scale_,
            static_cast<f32>(swapchain_extent_.height) / dpi_scale_};
}

f32 RHI::Impl::dpi_scale() const {
    return dpi_scale_;
}

// ---------------------------------------------------------------------------
// Offscreen rendering
// ---------------------------------------------------------------------------

bool RHI::Impl::create_offscreen_render_pass() {
    if (offscreen_render_pass_)
        return true;

    VkAttachmentDescription color_att{};
    color_att.format = swapchain_format_;
    color_att.samples = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 1;
    ci.pAttachments = &color_att;
    ci.subpassCount = 1;
    ci.pSubpasses = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;

    return vkCreateRenderPass(device_, &ci, nullptr, &offscreen_render_pass_) == VK_SUCCESS;
}

RHITextureHandle RHI::Impl::CreateRenderTarget(u32 width, u32 height) {
    if (!create_offscreen_render_pass())
        return kInvalidTexture;

    // Find a free slot
    RHITextureHandle handle = kInvalidTexture;
    for (u32 i = 0; i < MAX_TEXTURES; ++i) {
        if (!textures_[i].in_use) {
            handle = i;
            break;
        }
    }
    if (handle == kInvalidTexture)
        return kInvalidTexture;

    auto& slot = textures_[handle];

    // Create image with COLOR_ATTACHMENT + SAMPLED usage
    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent = {width, height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = swapchain_format_;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    if (vkCreateImage(device_, &ici, nullptr, &slot.image) != VK_SUCCESS)
        return kInvalidTexture;

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(device_, slot.image, &reqs);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = reqs.size;
    ai.memoryTypeIndex = find_memory_type(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &ai, nullptr, &slot.memory);
    vkBindImageMemory(device_, slot.image, slot.memory, 0);

    // Image view
    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = slot.image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = swapchain_format_;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(device_, &vci, nullptr, &slot.view);

    // Framebuffer for offscreen rendering
    VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = offscreen_render_pass_;
    fci.attachmentCount = 1;
    fci.pAttachments = &slot.view;
    fci.width = width;
    fci.height = height;
    fci.layers = 1;
    if (vkCreateFramebuffer(device_, &fci, nullptr, &slot.framebuffer) != VK_SUCCESS) {
        vkDestroyImageView(device_, slot.view, nullptr);
        vkDestroyImage(device_, slot.image, nullptr);
        vkFreeMemory(device_, slot.memory, nullptr);
        slot = {};
        return kInvalidTexture;
    }

    // Transition to SHADER_READ_ONLY so sampling before first render is safe
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = frames_[0].cmd_pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    vkAllocateCommandBuffers(device_, &cai, &cmd);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = slot.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(graphics_queue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);
    vkFreeCommandBuffers(device_, frames_[0].cmd_pool, 1, &cmd);

    // Descriptor set for sampling this texture
    VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = desc_pool_;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &desc_set_layout_;
    vkAllocateDescriptorSets(device_, &dai, &slot.descriptor);

    VkDescriptorImageInfo dii{};
    dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dii.imageView = slot.view;
    dii.sampler = default_sampler_;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = slot.descriptor;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &dii;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    slot.width = width;
    slot.height = height;
    slot.pixel_size = 4;
    slot.in_use = true;
    slot.is_render_target = true;
    return handle;
}

void RHI::Impl::DestroyRenderTarget(RHITextureHandle handle) {
    if (handle >= MAX_TEXTURES || !textures_[handle].in_use || !textures_[handle].is_render_target)
        return;
    vkDeviceWaitIdle(device_);
    auto& slot = textures_[handle];
    if (slot.framebuffer)
        vkDestroyFramebuffer(device_, slot.framebuffer, nullptr);
    vkDestroyImageView(device_, slot.view, nullptr);
    vkDestroyImage(device_, slot.image, nullptr);
    vkFreeMemory(device_, slot.memory, nullptr);
    slot = {};
}

bool RHI::Impl::BeginOffscreen(RHITextureHandle target, Color clear_color) {
    if (target >= MAX_TEXTURES || !textures_[target].in_use || !textures_[target].is_render_target)
        return false;

    auto& slot = textures_[target];
    auto& f = frames_[current_frame_];

    active_offscreen_target_ = target;
    offscreen_display_size_ = {static_cast<f32>(slot.width), static_cast<f32>(slot.height)};

    VkClearValue clear = {};
    clear.color = {{clear_color.r, clear_color.g, clear_color.b, clear_color.a}};

    VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = offscreen_render_pass_;
    rbi.framebuffer = slot.framebuffer;
    rbi.renderArea = {{0, 0}, {slot.width, slot.height}};
    rbi.clearValueCount = 1;
    rbi.pClearValues = &clear;
    vkCmdBeginRenderPass(f.cmd_buffer, &rbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(f.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{};
    viewport.width = static_cast<f32>(slot.width);
    viewport.height = static_cast<f32>(slot.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(f.cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {slot.width, slot.height}};
    vkCmdSetScissor(f.cmd_buffer, 0, 1, &scissor);

    // Orthographic projection for pixel-exact offscreen coordinates
    f32 push[4] = {
        2.0f / static_cast<f32>(slot.width),
        2.0f / static_cast<f32>(slot.height),
        -1.0f,
        -1.0f,
    };
    vkCmdPushConstants(f.cmd_buffer, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push),
                       push);

    return true;
}

void RHI::Impl::EndOffscreen(RHITextureHandle target) {
    if (active_offscreen_target_ == kInvalidTexture)
        return;

    auto& f = frames_[current_frame_];
    vkCmdEndRenderPass(f.cmd_buffer);

    // Pipeline barrier: ensure the offscreen render is complete before it is
    // sampled as a texture in a subsequent render pass.
    auto& slot = textures_[target];
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = slot.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(f.cmd_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);

    active_offscreen_target_ = kInvalidTexture;
}

// ---------------------------------------------------------------------------
// Video pipeline (YCbCr -> RGBA)
// ---------------------------------------------------------------------------

bool RHI::Impl::ensure_video_pipeline() {
    if (video_pipeline_ready_)
        return true;

    if (!create_offscreen_render_pass())
        return false;

    // Descriptor set layout: 3 combined image samplers (Y, Cb, Cr)
    VkDescriptorSetLayoutBinding bindings[3] = {};
    for (u32 i = 0; i < 3; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo dsl_ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl_ci.bindingCount = 3;
    dsl_ci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device_, &dsl_ci, nullptr, &video_desc_set_layout_) != VK_SUCCESS)
        return false;

    // Descriptor pool: one set per in-flight frame, 3 samplers each
    VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES * 3};
    VkDescriptorPoolCreateInfo dpc{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpc.poolSizeCount = 1;
    dpc.pPoolSizes = &pool_size;
    dpc.maxSets = MAX_FRAMES;
    if (vkCreateDescriptorPool(device_, &dpc, nullptr, &video_desc_pool_) != VK_SUCCESS)
        return false;

    // Allocate per-frame descriptor sets
    for (u32 i = 0; i < MAX_FRAMES; ++i) {
        VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = video_desc_pool_;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &video_desc_set_layout_;
        if (vkAllocateDescriptorSets(device_, &dai, &video_desc_sets_[i]) != VK_SUCCESS)
            return false;
    }

    // Pipeline layout (no push constants: fullscreen pass from gl_VertexIndex)
    VkPipelineLayoutCreateInfo plc{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plc.setLayoutCount = 1;
    plc.pSetLayouts = &video_desc_set_layout_;
    if (vkCreatePipelineLayout(device_, &plc, nullptr, &video_pipeline_layout_) != VK_SUCCESS)
        return false;

    // Shaders
    auto vert_mod = load_shader("video.vert.spv");
    auto frag_mod = load_shader("video.frag.spv");
    if (!vert_mod || !frag_mod) {
        if (vert_mod) vkDestroyShaderModule(device_, vert_mod, nullptr);
        if (frag_mod) vkDestroyShaderModule(device_, frag_mod, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_mod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    // No vertex input: vertices generated in shader from gl_VertexIndex
    VkPipelineVertexInputStateCreateInfo vertex_input{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    raster.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // No blending: opaque video frame
    VkPipelineColorBlendAttachmentState blend_att{};
    blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &blend_att;

    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dyn_states;

    VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vertex_input;
    pi.pInputAssemblyState = &input_assembly;
    pi.pViewportState = &viewport_state;
    pi.pRasterizationState = &raster;
    pi.pMultisampleState = &multisample;
    pi.pColorBlendState = &color_blend;
    pi.pDynamicState = &dynamic_state;
    pi.layout = video_pipeline_layout_;
    pi.renderPass = offscreen_render_pass_;
    pi.subpass = 0;

    VkResult result =
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &video_pipeline_);

    vkDestroyShaderModule(device_, vert_mod, nullptr);
    vkDestroyShaderModule(device_, frag_mod, nullptr);

    if (result != VK_SUCCESS)
        return false;

    video_pipeline_ready_ = true;
    return true;
}

void RHI::Impl::ConvertVideoFrame(RHITextureHandle target,
                                   RHITextureHandle y, RHITextureHandle cb,
                                   RHITextureHandle cr) {
    if (!ensure_video_pipeline())
        return;
    if (target >= MAX_TEXTURES || !textures_[target].in_use || !textures_[target].is_render_target)
        return;
    if (y >= MAX_TEXTURES || !textures_[y].in_use)
        return;
    if (cb >= MAX_TEXTURES || !textures_[cb].in_use)
        return;
    if (cr >= MAX_TEXTURES || !textures_[cr].in_use)
        return;

    auto& slot = textures_[target];
    auto& f = frames_[current_frame_];

    // Update the per-frame descriptor set with the 3 plane textures
    VkDescriptorImageInfo img_infos[3] = {};
    RHITextureHandle planes[3] = {y, cb, cr};
    for (u32 i = 0; i < 3; ++i) {
        img_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_infos[i].imageView = textures_[planes[i]].view;
        img_infos[i].sampler = default_sampler_;
    }

    VkWriteDescriptorSet writes[3] = {};
    for (u32 i = 0; i < 3; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = video_desc_sets_[current_frame_];
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &img_infos[i];
    }
    vkUpdateDescriptorSets(device_, 3, writes, 0, nullptr);

    // Begin offscreen render pass
    VkClearValue clear = {};
    VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = offscreen_render_pass_;
    rbi.framebuffer = slot.framebuffer;
    rbi.renderArea = {{0, 0}, {slot.width, slot.height}};
    rbi.clearValueCount = 1;
    rbi.pClearValues = &clear;
    vkCmdBeginRenderPass(f.cmd_buffer, &rbi, VK_SUBPASS_CONTENTS_INLINE);

    // Bind video pipeline and descriptor set
    vkCmdBindPipeline(f.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, video_pipeline_);
    vkCmdBindDescriptorSets(f.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            video_pipeline_layout_, 0, 1,
                            &video_desc_sets_[current_frame_], 0, nullptr);

    VkViewport viewport{};
    viewport.width = static_cast<f32>(slot.width);
    viewport.height = static_cast<f32>(slot.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(f.cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {slot.width, slot.height}};
    vkCmdSetScissor(f.cmd_buffer, 0, 1, &scissor);

    // Draw fullscreen triangle (3 vertices, no vertex buffer)
    vkCmdDraw(f.cmd_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(f.cmd_buffer);

    // Pipeline barrier: ensure the conversion is complete before sampling
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = slot.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(f.cmd_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void RHI::Impl::Shutdown() {
    s_rhi_instance = nullptr;

    if (device_)
        vkDeviceWaitIdle(device_);

    // Textures
    for (u32 i = 0; i < MAX_TEXTURES; ++i) {
        if (textures_[i].in_use)
            DestroyTexture(i);
    }

    if (default_sampler_)
        vkDestroySampler(device_, default_sampler_, nullptr);
    if (nearest_sampler_)
        vkDestroySampler(device_, nearest_sampler_, nullptr);

    // Per-swapchain-image semaphores
    for (u32 i = 0; i < MAX_SWAPCHAIN_IMAGES; ++i) {
        if (render_finished_[i])
            vkDestroySemaphore(device_, render_finished_[i], nullptr);
    }

    // Per-frame resources
    for (u32 i = 0; i < MAX_FRAMES; ++i) {
        auto& f = frames_[i];
        if (f.vertex_buffer)
            vkDestroyBuffer(device_, f.vertex_buffer, nullptr);
        if (f.vertex_memory)
            vkFreeMemory(device_, f.vertex_memory, nullptr);
        if (f.index_buffer)
            vkDestroyBuffer(device_, f.index_buffer, nullptr);
        if (f.index_memory)
            vkFreeMemory(device_, f.index_memory, nullptr);
        if (f.text_vertex_buffer)
            vkDestroyBuffer(device_, f.text_vertex_buffer, nullptr);
        if (f.text_vertex_memory)
            vkFreeMemory(device_, f.text_vertex_memory, nullptr);
        if (f.text_index_buffer)
            vkDestroyBuffer(device_, f.text_index_buffer, nullptr);
        if (f.text_index_memory)
            vkFreeMemory(device_, f.text_index_memory, nullptr);
        if (f.in_flight)
            vkDestroyFence(device_, f.in_flight, nullptr);
        if (f.image_available)
            vkDestroySemaphore(device_, f.image_available, nullptr);
        if (f.cmd_pool)
            vkDestroyCommandPool(device_, f.cmd_pool, nullptr);
    }

    if (video_pipeline_)
        vkDestroyPipeline(device_, video_pipeline_, nullptr);
    if (video_pipeline_layout_)
        vkDestroyPipelineLayout(device_, video_pipeline_layout_, nullptr);
    if (video_desc_pool_)
        vkDestroyDescriptorPool(device_, video_desc_pool_, nullptr);
    if (video_desc_set_layout_)
        vkDestroyDescriptorSetLayout(device_, video_desc_set_layout_, nullptr);

    if (desc_pool_)
        vkDestroyDescriptorPool(device_, desc_pool_, nullptr);
    if (pipeline_)
        vkDestroyPipeline(device_, pipeline_, nullptr);
    if (text_pipeline_)
        vkDestroyPipeline(device_, text_pipeline_, nullptr);
    if (pipeline_layout_)
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    if (desc_set_layout_)
        vkDestroyDescriptorSetLayout(device_, desc_set_layout_, nullptr);
    if (offscreen_render_pass_)
        vkDestroyRenderPass(device_, offscreen_render_pass_, nullptr);
    if (render_pass_)
        vkDestroyRenderPass(device_, render_pass_, nullptr);

    cleanup_swapchain();

    if (device_)
        vkDestroyDevice(device_, nullptr);
    if (surface_)
        vkDestroySurfaceKHR(instance_, surface_, nullptr);

    if (debug_messenger_) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (func)
            func(instance_, debug_messenger_, nullptr);
    }

    if (instance_)
        vkDestroyInstance(instance_, nullptr);
}

// ---------------------------------------------------------------------------
// RHI forwarding methods (PIMPL)
// ---------------------------------------------------------------------------

RHI::RHI() : impl_(new Impl()) {}
RHI::~RHI() { delete impl_; }

bool RHI::Init(const RHIConfig& config) { return impl_->Init(config); }
void RHI::Shutdown() { impl_->Shutdown(); }
bool RHI::BeginFrame(Color clear_color) { return impl_->BeginFrame(clear_color); }
void RHI::EndFrame() { impl_->EndFrame(); }
void RHI::SetScissor(Rect rect) { impl_->SetScissor(rect); }
void RHI::ResetScissor() { impl_->ResetScissor(); }
void RHI::DrawTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                         u32 index_count, RHITextureHandle texture) {
    impl_->DrawTriangles(vertices, vertex_count, indices, index_count, texture);
}
void RHI::DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                              u32 index_count, RHITextureHandle atlas_texture) {
    impl_->DrawTextTriangles(vertices, vertex_count, indices, index_count, atlas_texture);
}
RHITextureHandle RHI::CreateTexture(u32 width, u32 height, RHIFormat format,
                                     const void* pixels, RHIFilter filter) {
    return impl_->CreateTexture(width, height, format, pixels, filter);
}
void RHI::UpdateTexture(RHITextureHandle handle, const void* pixels) { impl_->UpdateTexture(handle, pixels); }
void RHI::DestroyTexture(RHITextureHandle handle) { impl_->DestroyTexture(handle); }
bool RHI::AcquireFrame() { return impl_->AcquireFrame(); }
RHITextureHandle RHI::CreateRenderTarget(u32 width, u32 height) { return impl_->CreateRenderTarget(width, height); }
void RHI::DestroyRenderTarget(RHITextureHandle handle) { impl_->DestroyRenderTarget(handle); }
bool RHI::BeginOffscreen(RHITextureHandle target, Color clear_color) { return impl_->BeginOffscreen(target, clear_color); }
void RHI::EndOffscreen(RHITextureHandle target) { impl_->EndOffscreen(target); }
void RHI::ConvertVideoFrame(RHITextureHandle target, RHITextureHandle y,
                              RHITextureHandle cb, RHITextureHandle cr) {
    impl_->ConvertVideoFrame(target, y, cb, cr);
}
Vec2 RHI::display_size() const { return impl_->display_size(); }
f32 RHI::dpi_scale() const { return impl_->dpi_scale(); }

#pragma clang diagnostic pop

} // namespace ugui
