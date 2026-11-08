#!/usr/bin/env python3
"""Generates src/sys/vulkan.aki and tests/vulkan_layout_test.aki.

The Vulkan binding of ultragui_vulkan is written out from the table below:
exactly the structs, functions and constants vulkan_rhi.cc,
ugui_impl_vulkan.cc and the examples use. Constant values and struct layouts
are not typed in: this script compiles a C program against the system's
vulkan.h that prints them, and writes the values into the binding and the
expected layouts into the layout test (which then checks akiko's
@size_of/@offset_of against C's sizeof/offsetof).

Run it from the repository root inside the dev shell:
  nix develop -c python3 aki/ultragui_vulkan/tools/gen_vk_binding.py
"""

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PKG = os.path.dirname(HERE)

# Dispatchable handles are pointers to opaque types; the rest are 64-bit
# integers (VK_DEFINE_NON_DISPATCHABLE_HANDLE on 64-bit targets).
DISPATCHABLE = ["VkInstance", "VkPhysicalDevice", "VkDevice", "VkQueue", "VkCommandBuffer"]
NON_DISPATCHABLE = [
    "VkSurfaceKHR", "VkSwapchainKHR", "VkImage", "VkImageView", "VkFramebuffer", "VkRenderPass",
    "VkDescriptorSetLayout", "VkPipelineLayout", "VkPipeline", "VkDescriptorPool", "VkDescriptorSet",
    "VkCommandPool", "VkSemaphore", "VkFence", "VkBuffer", "VkDeviceMemory", "VkSampler", "VkShaderModule",
    "VkDebugUtilsMessengerEXT", "VkPipelineCache", "VkBufferView",
]
# 32-bit enums and flags, named for the fields that hold them.
U32_ALIASES = [
    "VkBool32", "VkFlags", "VkStructureType", "VkFormat", "VkColorSpaceKHR", "VkPresentModeKHR",
    "VkImageUsageFlags", "VkSharingMode", "VkSurfaceTransformFlagBitsKHR", "VkSurfaceTransformFlagsKHR",
    "VkCompositeAlphaFlagBitsKHR", "VkCompositeAlphaFlagsKHR", "VkImageViewType", "VkComponentSwizzle",
    "VkImageAspectFlags", "VkSampleCountFlagBits", "VkAttachmentLoadOp", "VkAttachmentStoreOp", "VkImageLayout",
    "VkPipelineBindPoint", "VkPipelineStageFlags", "VkAccessFlags", "VkDependencyFlags", "VkDescriptorType",
    "VkShaderStageFlags", "VkShaderStageFlagBits", "VkVertexInputRate", "VkPrimitiveTopology", "VkPolygonMode",
    "VkCullModeFlags", "VkFrontFace", "VkBlendFactor", "VkBlendOp", "VkColorComponentFlags", "VkLogicOp",
    "VkDynamicState", "VkCommandPoolCreateFlags", "VkCommandBufferLevel", "VkFenceCreateFlags",
    "VkBufferUsageFlags", "VkMemoryPropertyFlags", "VkMemoryHeapFlags", "VkFilter", "VkSamplerMipmapMode",
    "VkSamplerAddressMode", "VkCompareOp", "VkBorderColor", "VkImageType", "VkImageTiling",
    "VkImageCreateFlags", "VkCommandBufferUsageFlags", "VkSubpassContents", "VkIndexType", "VkQueueFlags",
    "VkPhysicalDeviceType", "VkDebugUtilsMessageSeverityFlagsEXT", "VkDebugUtilsMessageSeverityFlagBitsEXT",
    "VkDebugUtilsMessageTypeFlagsEXT", "VkCommandBufferResetFlags", "VkMemoryMapFlags",
]

# struct name, sType constant (or None), fields as "C type  name" lines.
STRUCTS = [
    ("VkExtent2D", None, "uint32_t width; uint32_t height"),
    ("VkExtent3D", None, "uint32_t width; uint32_t height; uint32_t depth"),
    ("VkOffset2D", None, "int32_t x; int32_t y"),
    ("VkOffset3D", None, "int32_t x; int32_t y; int32_t z"),
    ("VkRect2D", None, "VkOffset2D offset; VkExtent2D extent"),
    ("VkApplicationInfo", "VK_STRUCTURE_TYPE_APPLICATION_INFO",
     "VkStructureType sType; const void* pNext; const char* pApplicationName; uint32_t applicationVersion; "
     "const char* pEngineName; uint32_t engineVersion; uint32_t apiVersion"),
    ("VkInstanceCreateInfo", "VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; const VkApplicationInfo* pApplicationInfo; "
     "uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames; uint32_t enabledExtensionCount; "
     "const char* const* ppEnabledExtensionNames"),
    ("VkDebugUtilsMessengerCreateInfoEXT", "VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT",
     "VkStructureType sType; const void* pNext; VkFlags flags; "
     "VkDebugUtilsMessageSeverityFlagsEXT messageSeverity; VkDebugUtilsMessageTypeFlagsEXT messageType; "
     "PFN_vkDebugUtilsMessengerCallbackEXT pfnUserCallback; void* pUserData"),
    # Read through a pointer only: declared through pMessage.
    ("VkDebugUtilsMessengerCallbackDataEXT", None,
     "VkStructureType sType; const void* pNext; VkFlags flags; const char* pMessageIdName; "
     "int32_t messageIdNumber; const char* pMessage", "prefix"),
    ("VkQueueFamilyProperties", None,
     "VkQueueFlags queueFlags; uint32_t queueCount; uint32_t timestampValidBits; "
     "VkExtent3D minImageTransferGranularity"),
    # Written whole by the driver: the limits and sparse properties after
    # pipelineCacheUUID are kept as opaque storage.
    ("VkPhysicalDeviceProperties", None,
     "uint32_t apiVersion; uint32_t driverVersion; uint32_t vendorID; uint32_t deviceID; "
     "VkPhysicalDeviceType deviceType; char deviceName[256]; uint8_t pipelineCacheUUID[16]; "
     "uint64_t limits_and_sparse[66]", "opaque_tail"),
    ("VkDeviceQueueCreateInfo", "VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint32_t queueFamilyIndex; uint32_t queueCount; "
     "const float* pQueuePriorities"),
    ("VkPhysicalDeviceFeatures", None, "VkBool32 features[55]", "opaque_tail"),
    ("VkDeviceCreateInfo", "VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint32_t queueCreateInfoCount; "
     "const VkDeviceQueueCreateInfo* pQueueCreateInfos; uint32_t enabledLayerCount; "
     "const char* const* ppEnabledLayerNames; uint32_t enabledExtensionCount; "
     "const char* const* ppEnabledExtensionNames; const VkPhysicalDeviceFeatures* pEnabledFeatures"),
    ("VkSurfaceCapabilitiesKHR", None,
     "uint32_t minImageCount; uint32_t maxImageCount; VkExtent2D currentExtent; VkExtent2D minImageExtent; "
     "VkExtent2D maxImageExtent; uint32_t maxImageArrayLayers; VkSurfaceTransformFlagsKHR supportedTransforms; "
     "VkSurfaceTransformFlagBitsKHR currentTransform; VkCompositeAlphaFlagsKHR supportedCompositeAlpha; "
     "VkImageUsageFlags supportedUsageFlags"),
    ("VkSurfaceFormatKHR", None, "VkFormat format; VkColorSpaceKHR colorSpace"),
    ("VkSwapchainCreateInfoKHR", "VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR",
     "VkStructureType sType; const void* pNext; VkFlags flags; VkSurfaceKHR surface; uint32_t minImageCount; "
     "VkFormat imageFormat; VkColorSpaceKHR imageColorSpace; VkExtent2D imageExtent; uint32_t imageArrayLayers; "
     "VkImageUsageFlags imageUsage; VkSharingMode imageSharingMode; uint32_t queueFamilyIndexCount; "
     "const uint32_t* pQueueFamilyIndices; VkSurfaceTransformFlagBitsKHR preTransform; "
     "VkCompositeAlphaFlagBitsKHR compositeAlpha; VkPresentModeKHR presentMode; VkBool32 clipped; "
     "VkSwapchainKHR oldSwapchain"),
    ("VkComponentMapping", None,
     "VkComponentSwizzle r; VkComponentSwizzle g; VkComponentSwizzle b; VkComponentSwizzle a"),
    ("VkImageSubresourceRange", None,
     "VkImageAspectFlags aspectMask; uint32_t baseMipLevel; uint32_t levelCount; uint32_t baseArrayLayer; "
     "uint32_t layerCount"),
    ("VkImageSubresourceLayers", None,
     "VkImageAspectFlags aspectMask; uint32_t mipLevel; uint32_t baseArrayLayer; uint32_t layerCount"),
    ("VkImageViewCreateInfo", "VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; VkImage image; VkImageViewType viewType; "
     "VkFormat format; VkComponentMapping components; VkImageSubresourceRange subresourceRange"),
    ("VkAttachmentDescription", None,
     "VkFlags flags; VkFormat format; VkSampleCountFlagBits samples; VkAttachmentLoadOp loadOp; "
     "VkAttachmentStoreOp storeOp; VkAttachmentLoadOp stencilLoadOp; VkAttachmentStoreOp stencilStoreOp; "
     "VkImageLayout initialLayout; VkImageLayout finalLayout"),
    ("VkAttachmentReference", None, "uint32_t attachment; VkImageLayout layout"),
    ("VkSubpassDescription", None,
     "VkFlags flags; VkPipelineBindPoint pipelineBindPoint; uint32_t inputAttachmentCount; "
     "const VkAttachmentReference* pInputAttachments; uint32_t colorAttachmentCount; "
     "const VkAttachmentReference* pColorAttachments; const VkAttachmentReference* pResolveAttachments; "
     "const VkAttachmentReference* pDepthStencilAttachment; uint32_t preserveAttachmentCount; "
     "const uint32_t* pPreserveAttachments"),
    ("VkSubpassDependency", None,
     "uint32_t srcSubpass; uint32_t dstSubpass; VkPipelineStageFlags srcStageMask; "
     "VkPipelineStageFlags dstStageMask; VkAccessFlags srcAccessMask; VkAccessFlags dstAccessMask; "
     "VkDependencyFlags dependencyFlags"),
    ("VkRenderPassCreateInfo", "VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint32_t attachmentCount; "
     "const VkAttachmentDescription* pAttachments; uint32_t subpassCount; const VkSubpassDescription* pSubpasses; "
     "uint32_t dependencyCount; const VkSubpassDependency* pDependencies"),
    ("VkDescriptorSetLayoutBinding", None,
     "uint32_t binding; VkDescriptorType descriptorType; uint32_t descriptorCount; "
     "VkShaderStageFlags stageFlags; const VkSampler* pImmutableSamplers"),
    ("VkDescriptorSetLayoutCreateInfo", "VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint32_t bindingCount; "
     "const VkDescriptorSetLayoutBinding* pBindings"),
    ("VkDescriptorPoolSize", None, "VkDescriptorType type; uint32_t descriptorCount"),
    ("VkDescriptorPoolCreateInfo", "VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint32_t maxSets; uint32_t poolSizeCount; "
     "const VkDescriptorPoolSize* pPoolSizes"),
    ("VkShaderModuleCreateInfo", "VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; size_t codeSize; const uint32_t* pCode"),
    ("VkPipelineShaderStageCreateInfo", "VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; VkShaderStageFlagBits stage; "
     "VkShaderModule module; const char* pName; const void* pSpecializationInfo"),
    ("VkVertexInputBindingDescription", None,
     "uint32_t binding; uint32_t stride; VkVertexInputRate inputRate"),
    ("VkVertexInputAttributeDescription", None,
     "uint32_t location; uint32_t binding; VkFormat format; uint32_t offset"),
    ("VkPipelineVertexInputStateCreateInfo", "VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint32_t vertexBindingDescriptionCount; "
     "const VkVertexInputBindingDescription* pVertexBindingDescriptions; "
     "uint32_t vertexAttributeDescriptionCount; "
     "const VkVertexInputAttributeDescription* pVertexAttributeDescriptions"),
    ("VkPipelineInputAssemblyStateCreateInfo", "VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; VkPrimitiveTopology topology; "
     "VkBool32 primitiveRestartEnable"),
    ("VkViewport", None,
     "float x; float y; float width; float height; float minDepth; float maxDepth"),
    ("VkPipelineViewportStateCreateInfo", "VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint32_t viewportCount; "
     "const VkViewport* pViewports; uint32_t scissorCount; const VkRect2D* pScissors"),
    ("VkPipelineRasterizationStateCreateInfo", "VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; VkBool32 depthClampEnable; "
     "VkBool32 rasterizerDiscardEnable; VkPolygonMode polygonMode; VkCullModeFlags cullMode; "
     "VkFrontFace frontFace; VkBool32 depthBiasEnable; float depthBiasConstantFactor; float depthBiasClamp; "
     "float depthBiasSlopeFactor; float lineWidth"),
    ("VkPipelineMultisampleStateCreateInfo", "VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; VkSampleCountFlagBits rasterizationSamples; "
     "VkBool32 sampleShadingEnable; float minSampleShading; const uint32_t* pSampleMask; "
     "VkBool32 alphaToCoverageEnable; VkBool32 alphaToOneEnable"),
    ("VkPipelineColorBlendAttachmentState", None,
     "VkBool32 blendEnable; VkBlendFactor srcColorBlendFactor; VkBlendFactor dstColorBlendFactor; "
     "VkBlendOp colorBlendOp; VkBlendFactor srcAlphaBlendFactor; VkBlendFactor dstAlphaBlendFactor; "
     "VkBlendOp alphaBlendOp; VkColorComponentFlags colorWriteMask"),
    ("VkPipelineColorBlendStateCreateInfo", "VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; VkBool32 logicOpEnable; VkLogicOp logicOp; "
     "uint32_t attachmentCount; const VkPipelineColorBlendAttachmentState* pAttachments; "
     "float blendConstants[4]"),
    ("VkPipelineDynamicStateCreateInfo", "VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint32_t dynamicStateCount; "
     "const VkDynamicState* pDynamicStates"),
    ("VkPushConstantRange", None, "VkShaderStageFlags stageFlags; uint32_t offset; uint32_t size"),
    ("VkPipelineLayoutCreateInfo", "VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint32_t setLayoutCount; "
     "const VkDescriptorSetLayout* pSetLayouts; uint32_t pushConstantRangeCount; "
     "const VkPushConstantRange* pPushConstantRanges"),
    ("VkGraphicsPipelineCreateInfo", "VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint32_t stageCount; "
     "const VkPipelineShaderStageCreateInfo* pStages; "
     "const VkPipelineVertexInputStateCreateInfo* pVertexInputState; "
     "const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState; const void* pTessellationState; "
     "const VkPipelineViewportStateCreateInfo* pViewportState; "
     "const VkPipelineRasterizationStateCreateInfo* pRasterizationState; "
     "const VkPipelineMultisampleStateCreateInfo* pMultisampleState; const void* pDepthStencilState; "
     "const VkPipelineColorBlendStateCreateInfo* pColorBlendState; "
     "const VkPipelineDynamicStateCreateInfo* pDynamicState; VkPipelineLayout layout; VkRenderPass renderPass; "
     "uint32_t subpass; VkPipeline basePipelineHandle; int32_t basePipelineIndex"),
    ("VkFramebufferCreateInfo", "VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; VkRenderPass renderPass; "
     "uint32_t attachmentCount; const VkImageView* pAttachments; uint32_t width; uint32_t height; "
     "uint32_t layers"),
    ("VkCommandPoolCreateInfo", "VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkCommandPoolCreateFlags flags; uint32_t queueFamilyIndex"),
    ("VkCommandBufferAllocateInfo", "VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO",
     "VkStructureType sType; const void* pNext; VkCommandPool commandPool; VkCommandBufferLevel level; "
     "uint32_t commandBufferCount"),
    ("VkSemaphoreCreateInfo", "VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags"),
    ("VkFenceCreateInfo", "VK_STRUCTURE_TYPE_FENCE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFenceCreateFlags flags"),
    ("VkMemoryType", None, "VkMemoryPropertyFlags propertyFlags; uint32_t heapIndex"),
    ("VkMemoryHeap", None, "uint64_t size; VkMemoryHeapFlags flags"),
    ("VkPhysicalDeviceMemoryProperties", None,
     "uint32_t memoryTypeCount; VkMemoryType memoryTypes[32]; uint32_t memoryHeapCount; "
     "VkMemoryHeap memoryHeaps[16]"),
    ("VkBufferCreateInfo", "VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; uint64_t size; VkBufferUsageFlags usage; "
     "VkSharingMode sharingMode; uint32_t queueFamilyIndexCount; const uint32_t* pQueueFamilyIndices"),
    ("VkMemoryRequirements", None, "uint64_t size; uint64_t alignment; uint32_t memoryTypeBits"),
    ("VkMemoryAllocateInfo", "VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO",
     "VkStructureType sType; const void* pNext; uint64_t allocationSize; uint32_t memoryTypeIndex"),
    ("VkSamplerCreateInfo", "VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkFlags flags; VkFilter magFilter; VkFilter minFilter; "
     "VkSamplerMipmapMode mipmapMode; VkSamplerAddressMode addressModeU; VkSamplerAddressMode addressModeV; "
     "VkSamplerAddressMode addressModeW; float mipLodBias; VkBool32 anisotropyEnable; float maxAnisotropy; "
     "VkBool32 compareEnable; VkCompareOp compareOp; float minLod; float maxLod; VkBorderColor borderColor; "
     "VkBool32 unnormalizedCoordinates"),
    ("VkImageCreateInfo", "VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO",
     "VkStructureType sType; const void* pNext; VkImageCreateFlags flags; VkImageType imageType; "
     "VkFormat format; VkExtent3D extent; uint32_t mipLevels; uint32_t arrayLayers; "
     "VkSampleCountFlagBits samples; VkImageTiling tiling; VkImageUsageFlags usage; VkSharingMode sharingMode; "
     "uint32_t queueFamilyIndexCount; const uint32_t* pQueueFamilyIndices; VkImageLayout initialLayout"),
    ("VkCommandBufferBeginInfo", "VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO",
     "VkStructureType sType; const void* pNext; VkCommandBufferUsageFlags flags; const void* pInheritanceInfo"),
    ("VkImageMemoryBarrier", "VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER",
     "VkStructureType sType; const void* pNext; VkAccessFlags srcAccessMask; VkAccessFlags dstAccessMask; "
     "VkImageLayout oldLayout; VkImageLayout newLayout; uint32_t srcQueueFamilyIndex; "
     "uint32_t dstQueueFamilyIndex; VkImage image; VkImageSubresourceRange subresourceRange"),
    ("VkBufferImageCopy", None,
     "uint64_t bufferOffset; uint32_t bufferRowLength; uint32_t bufferImageHeight; "
     "VkImageSubresourceLayers imageSubresource; VkOffset3D imageOffset; VkExtent3D imageExtent"),
    ("VkSubmitInfo", "VK_STRUCTURE_TYPE_SUBMIT_INFO",
     "VkStructureType sType; const void* pNext; uint32_t waitSemaphoreCount; const VkSemaphore* pWaitSemaphores; "
     "const VkPipelineStageFlags* pWaitDstStageMask; uint32_t commandBufferCount; "
     "const VkCommandBuffer* pCommandBuffers; uint32_t signalSemaphoreCount; const VkSemaphore* pSignalSemaphores"),
    ("VkDescriptorSetAllocateInfo", "VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO",
     "VkStructureType sType; const void* pNext; VkDescriptorPool descriptorPool; uint32_t descriptorSetCount; "
     "const VkDescriptorSetLayout* pSetLayouts"),
    ("VkDescriptorImageInfo", None, "VkSampler sampler; VkImageView imageView; VkImageLayout imageLayout"),
    ("VkWriteDescriptorSet", "VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET",
     "VkStructureType sType; const void* pNext; VkDescriptorSet dstSet; uint32_t dstBinding; "
     "uint32_t dstArrayElement; uint32_t descriptorCount; VkDescriptorType descriptorType; "
     "const VkDescriptorImageInfo* pImageInfo; const void* pBufferInfo; const VkBufferView* pTexelBufferView"),
    # VkClearValue is a union of VkClearColorValue (float/int/uint [4]) and
    # VkClearDepthStencilValue; ultragui only clears colour, as floats.
    ("VkClearValue", None, "float float32[4]", "union"),
    ("VkRenderPassBeginInfo", "VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO",
     "VkStructureType sType; const void* pNext; VkRenderPass renderPass; VkFramebuffer framebuffer; "
     "VkRect2D renderArea; uint32_t clearValueCount; const VkClearValue* pClearValues"),
    ("VkPresentInfoKHR", "VK_STRUCTURE_TYPE_PRESENT_INFO_KHR",
     "VkStructureType sType; const void* pNext; uint32_t waitSemaphoreCount; const VkSemaphore* pWaitSemaphores; "
     "uint32_t swapchainCount; const VkSwapchainKHR* pSwapchains; const uint32_t* pImageIndices; "
     "VkResult* pResults"),
]

# Functions: return type, C name, parameters ("C type name", comma separated).
FUNCTIONS = """
VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
void vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator)
PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName)
VkResult vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices)
void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties)
VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported)
void vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties)
void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties)
VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
void vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue)
VkResult vkDeviceWaitIdle(VkDevice device)
VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities)
VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats)
VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes)
void vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator)
VkResult vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator)
VkResult vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages)
VkResult vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex)
VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
VkResult vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView)
void vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator)
VkResult vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer)
void vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator)
VkResult vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass)
void vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator)
VkResult vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout)
void vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator)
VkResult vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool)
void vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator)
VkResult vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets)
void vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const void* pDescriptorCopies)
VkResult vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule)
void vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator)
VkResult vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout)
void vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator)
VkResult vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines)
void vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator)
VkResult vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool)
void vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator)
VkResult vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers)
void vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo)
VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer)
VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags)
VkResult vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore)
void vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator)
VkResult vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence)
void vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator)
VkResult vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout)
VkResult vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences)
VkResult vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer)
void vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator)
void vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements)
VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, uint64_t memoryOffset)
VkResult vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage)
void vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator)
void vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements)
VkResult vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, uint64_t memoryOffset)
VkResult vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory)
void vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator)
VkResult vkMapMemory(VkDevice device, VkDeviceMemory memory, uint64_t offset, uint64_t size, VkMemoryMapFlags flags, void** ppData)
void vkUnmapMemory(VkDevice device, VkDeviceMemory memory)
VkResult vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler)
void vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator)
VkResult vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
VkResult vkQueueWaitIdle(VkQueue queue)
void vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const void* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const void* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
void vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions)
void vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents)
void vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
void vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
void vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports)
void vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors)
void vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues)
void vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets)
void vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const uint64_t* pOffsets)
void vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, uint64_t offset, VkIndexType indexType)
void vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
void vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
"""

# GLFW's Vulkan entry points (GLFW_INCLUDE_VULKAN), in the same module.
GLFW_FUNCTIONS = """
int glfwVulkanSupported()
const char** glfwGetRequiredInstanceExtensions(uint32_t* count)
VkResult glfwCreateWindowSurface(VkInstance instance, GLFWwindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface)
"""

CONSTANTS = """
VK_SUCCESS VK_SUBOPTIMAL_KHR VK_ERROR_OUT_OF_DATE_KHR
VK_TRUE VK_FALSE VK_QUEUE_FAMILY_IGNORED VK_SUBPASS_EXTERNAL
VK_API_VERSION_1_1 VK_API_VERSION_1_2
VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT VK_ACCESS_SHADER_READ_BIT VK_ACCESS_TRANSFER_WRITE_BIT
VK_ATTACHMENT_LOAD_OP_CLEAR VK_ATTACHMENT_LOAD_OP_DONT_CARE VK_ATTACHMENT_STORE_OP_DONT_CARE VK_ATTACHMENT_STORE_OP_STORE
VK_BLEND_FACTOR_ONE VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA VK_BLEND_FACTOR_SRC_ALPHA VK_BLEND_OP_ADD
VK_BUFFER_USAGE_INDEX_BUFFER_BIT VK_BUFFER_USAGE_TRANSFER_SRC_BIT VK_BUFFER_USAGE_TRANSFER_DST_BIT VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
VK_COLOR_COMPONENT_A_BIT VK_COLOR_COMPONENT_B_BIT VK_COLOR_COMPONENT_G_BIT VK_COLOR_COMPONENT_R_BIT
VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VK_COMMAND_BUFFER_LEVEL_PRIMARY VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR VK_CULL_MODE_NONE
VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER VK_DYNAMIC_STATE_SCISSOR VK_DYNAMIC_STATE_VIEWPORT
VK_FENCE_CREATE_SIGNALED_BIT VK_FILTER_LINEAR VK_FILTER_NEAREST
VK_FORMAT_UNDEFINED VK_FORMAT_B8G8R8A8_SRGB VK_FORMAT_B8G8R8A8_UNORM VK_FORMAT_R8G8B8A8_SRGB VK_FORMAT_R8G8B8A8_UNORM
VK_FORMAT_R32G32B32A32_SFLOAT VK_FORMAT_R32G32_SFLOAT VK_FORMAT_R32_SFLOAT VK_FORMAT_R32_UINT VK_FORMAT_R8_UNORM
VK_FRONT_FACE_COUNTER_CLOCKWISE VK_IMAGE_ASPECT_COLOR_BIT VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
VK_IMAGE_LAYOUT_UNDEFINED VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
VK_IMAGE_TILING_OPTIMAL VK_IMAGE_TYPE_2D
VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT VK_IMAGE_USAGE_SAMPLED_BIT VK_IMAGE_USAGE_TRANSFER_DST_BIT VK_IMAGE_USAGE_TRANSFER_SRC_BIT
VK_IMAGE_VIEW_TYPE_2D VK_INDEX_TYPE_UINT32
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT VK_MEMORY_PROPERTY_HOST_COHERENT_BIT VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
VK_PIPELINE_BIND_POINT_GRAPHICS
VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
VK_PIPELINE_STAGE_TRANSFER_BIT VK_POLYGON_MODE_FILL VK_PRESENT_MODE_FIFO_KHR VK_PRESENT_MODE_MAILBOX_KHR
VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST VK_QUEUE_GRAPHICS_BIT VK_SAMPLE_COUNT_1_BIT VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
VK_SHADER_STAGE_FRAGMENT_BIT VK_SHADER_STAGE_VERTEX_BIT VK_SHARING_MODE_CONCURRENT VK_SHARING_MODE_EXCLUSIVE
VK_SUBPASS_CONTENTS_INLINE VK_VERTEX_INPUT_RATE_VERTEX
""".split()

SIGNED_CONSTANTS = {"VK_SUCCESS", "VK_SUBOPTIMAL_KHR", "VK_ERROR_OUT_OF_DATE_KHR"}


def snake(name):
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name)
    s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", s)
    return s.lower()


def field_name(c):
    if c == "sType":
        return "s_type"
    m = re.match(r"^(pfn|pp|p)([A-Z].*)$", c)
    if m:
        c = m.group(2)
    n = snake(c)
    return {"module": "module_", "type": "type_", "layout": "layout", "size": "size"}.get(n, n)


def const_name(c):
    return "k" + "".join(seg.capitalize() for seg in c.split("_"))


def fn_name(c):
    return snake(c).replace("k_h_r", "khr")


def parse_decl(d):
    d = d.strip()
    m = re.match(r"^(.*?)\s*\b(\w+)\s*(\[(\d+)\])?$", d)
    ctype, name, arr = m.group(1).strip(), m.group(2), m.group(4)
    return ctype, name, int(arr) if arr else None


def aki_type(ctype):
    toks = re.findall(r"const|\*|\w+", ctype)
    i = 0
    pointee_const = False
    while toks[i] == "const":
        pointee_const = True
        i += 1
    base = toks[i]
    i += 1
    if i < len(toks) and toks[i] == "const":
        pointee_const = True
        i += 1
    if i == len(toks):
        return base_type(base)
    t = {"void": "u8", "char": "c_char"}.get(base) or base_type(base)
    while i < len(toks):
        assert toks[i] == "*", ctype
        i += 1
        t = ("*const " if pointee_const else "*var ") + t
        pointee_const = False
        if i < len(toks) and toks[i] == "const":
            pointee_const = True
            i += 1
    return t


def base_type(t):
    base = {
        "uint32_t": "u32", "int32_t": "i32", "uint64_t": "u64", "uint8_t": "u8", "float": "f32",
        "size_t": "usize", "char": "c_char", "int": "i32", "VkResult": "VkResult",
        "PFN_vkDebugUtilsMessengerCallbackEXT": "VkDebugUtilsMessengerCallback?",
        "PFN_vkVoidFunction": "VkVoidFunction?", "GLFWwindow": "GlfwWindow",
    }
    if t in base:
        return base[t]
    if t.startswith("Vk"):
        return t
    raise SystemExit("unknown C type " + t)


def default_of(atype, arr):
    if arr:
        return "[%s; %d]" % (default_of(atype, None), arr)
    if atype.startswith("*"):
        return "null"
    if atype.endswith("?"):
        return "none"
    if atype == "f32":
        return "0.0"
    if atype in ("u32", "i32", "u64", "u8", "usize", "VkResult") or atype in U32_ALIASES or atype in NON_DISPATCHABLE:
        return "0"
    if atype == "c_char":
        return "0 as c_char"
    if atype in DISPATCHABLE:
        return "null"
    return atype + " {}"


def run_c(program, cflags):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "probe.c")
        exe = os.path.join(d, "probe")
        with open(src, "w") as f:
            f.write(program)
        subprocess.check_call(["clang", "-std=c11", "-o", exe, src] + cflags)
        return subprocess.check_output([exe]).decode()


def main():
    structs = []
    for entry in STRUCTS:
        name, stype, fields = entry[0], entry[1], entry[2]
        kind = entry[3] if len(entry) > 3 else "whole"
        decls = [parse_decl(f) for f in fields.split(";")]
        structs.append((name, stype, decls, kind))

    # --- probe C ---
    probe = ["#include <stdio.h>", "#include <stddef.h>", "#include <vulkan/vulkan.h>", "int main(void) {"]
    for c in CONSTANTS:
        if c in SIGNED_CONSTANTS:
            probe.append('printf("C %s %%lld\\n", (long long)(%s));' % (c, c))
        else:
            probe.append('printf("C %s %%llu\\n", (unsigned long long)(unsigned)(%s));' % (c, c))
    for name, stype, decls, kind in structs:
        if stype:
            probe.append('printf("C %s %%llu\\n", (unsigned long long)(%s));' % (stype, stype))
        if kind in ("whole", "union", "opaque_tail"):
            probe.append('printf("S %s %%zu %%zu\\n", sizeof(%s), _Alignof(%s));' % (name, name, name))
        if kind in ("whole", "prefix"):
            for ctype, fname, arr in decls:
                probe.append('printf("O %s %s %%zu\\n", offsetof(%s, %s));' % (name, fname, name, fname))
        elif kind == "opaque_tail":
            for ctype, fname, arr in decls[:-1]:
                probe.append('printf("O %s %s %%zu\\n", offsetof(%s, %s));' % (name, fname, name, fname))
    probe.append("return 0; }")
    out = run_c("\n".join(probe), [])
    consts, sizes, offsets = {}, {}, {}
    for line in out.splitlines():
        p = line.split()
        if p[0] == "C":
            consts[p[1]] = int(p[2])
        elif p[0] == "S":
            sizes[p[1]] = (int(p[2]), int(p[3]))
        elif p[0] == "O":
            offsets[(p[1], p[2])] = int(p[3])

    # --- binding ---
    o = []
    w = o.append
    w("// GENERATED by tools/gen_vk_binding.py from the table there; do not edit.")
    w("// No C++ counterpart: the Vulkan 1.2 API (vulkan/vulkan.h) and GLFW's")
    w("// Vulkan entry points that vulkan_rhi, ugui_impl_vulkan and the examples")
    w("// call, declared by hand. Layouts are C's (`extern struct`), field names")
    w("// are snake_case without Vulkan's p/pp/pfn pointer prefixes, and every")
    w("// field defaults to zero/null, so `VkFooCreateInfo { ... }` is C++'s")
    w("// `VkFooCreateInfo ci{VK_STRUCTURE_TYPE_FOO_CREATE_INFO}` (s_type defaults")
    w("// to the struct's own type). Constant values and the layouts were taken")
    w("// from vulkan.h by the generator and are checked by")
    w("// tests/vulkan_layout_test.aki. 64-bit targets only (non-dispatchable")
    w("// handles are u64).")
    w("")
    w("module ultragui_vulkan::sys::vulkan;")
    w("")
    w("import {GlfwWindow} from ultragui_glfw::sys::glfw;")
    w("")
    w("// Dispatchable handles: pointers to opaque driver objects.")
    for h in DISPATCHABLE:
        w("pub extern type %sT;" % h)
        w("pub type %s = *var %sT;" % (h, h))
    w("")
    w("// Non-dispatchable handles: 64-bit values, VK_NULL_HANDLE is 0.")
    for h in NON_DISPATCHABLE:
        w("pub type %s = u64;" % h)
    w("pub comptime let kVkNullHandle: u64 = 0;")
    w("")
    w("// Enums and flags, 32 bits wide.")
    for a in U32_ALIASES:
        w("pub type %s = u32;" % a)
    w("pub type VkResult = i32;")
    w("")
    w("pub extern type VkAllocationCallbacks;")
    w("")
    w("// PFN_vkVoidFunction, PFN_vkDebugUtilsMessengerCallbackEXT")
    w("pub type VkVoidFunction = extern fn(): ();")
    w("pub type VkDebugUtilsMessengerCallback = extern fn(u32, u32, *const VkDebugUtilsMessengerCallbackDataEXT, *var u8): u32;")
    w("pub type VkCreateDebugUtilsMessengerFn = extern fn(VkInstance, *const VkDebugUtilsMessengerCreateInfoEXT, *const VkAllocationCallbacks, *var VkDebugUtilsMessengerEXT): VkResult;")
    w("pub type VkDestroyDebugUtilsMessengerFn = extern fn(VkInstance, VkDebugUtilsMessengerEXT, *const VkAllocationCallbacks): ();")
    w("")
    w("// Constants")
    for c in CONSTANTS:
        t = "VkResult" if c in SIGNED_CONSTANTS else "u32"
        w("pub comptime let %s: %s = %d;" % (const_name(c), t, consts[c]))
    w("pub comptime let kVkWholeSize: u64 = 0xFFFF_FFFF_FFFF_FFFF;")
    w('pub comptime let kVkKhrSwapchainExtensionName: str = "VK_KHR_swapchain";')
    w('pub comptime let kVkExtDebugUtilsExtensionName: str = "VK_EXT_debug_utils";')
    w("")
    w("// VK_MAKE_VERSION(major, minor, patch)")
    w("pub fn vk_make_version(major: u32, minor: u32, patch: u32): u32 => (major << 22) | (minor << 12) | patch;")
    w("")
    w("// Structure types")
    stypes = sorted({s for _, s, _, _ in structs if s})
    for s in stypes:
        w("pub comptime let %s: VkStructureType = %d;" % (const_name(s), consts[s]))
    w("")
    for name, stype, decls, kind in structs:
        if kind == "prefix":
            w("// Through the last field read: a prefix of the C struct.")
        if kind == "opaque_tail":
            w("// The fields after the named ones are opaque storage of the C size.")
        w("pub extern struct %s {" % name)
        for ctype, fname, arr in decls:
            at = aki_type(ctype)
            ty = "[%s; %d]" % (at, arr) if arr else at
            if fname == "sType" and stype:
                dv = const_name(stype)
            else:
                dv = default_of(at, arr)
            w("  pub var %s: %s = %s" % (field_name(fname), ty, dv))
        w("}")
        w("")

    def emit_fn(line):
        m = re.match(r"^(.*?)\s*\b(\w+)\((.*)\)$", line.strip())
        ret, cname, params = m.group(1), m.group(2), m.group(3)
        ps = []
        for p in [x for x in params.split(",") if x.strip()]:
            ctype, pname, arr = parse_decl(p)
            ps.append("%s: %s" % (field_name(pname), aki_type(ctype)))
        r = "()" if ret == "void" else aki_type(ret)
        w('pub extern("%s") fn %s(%s): %s;' % (cname, fn_name(cname), ", ".join(ps), r))

    w("// Functions")
    for line in FUNCTIONS.strip().splitlines():
        emit_fn(line)
    w("")
    w("// GLFW's Vulkan entry points")
    for line in GLFW_FUNCTIONS.strip().splitlines():
        emit_fn(line)

    with open(os.path.join(PKG, "src", "sys", "vulkan.aki"), "w") as f:
        f.write("\n".join(o) + "\n")

    # --- layout test ---
    t = []
    w = t.append
    w("// GENERATED by tools/gen_vk_binding.py; do not edit.")
    w("// The Vulkan binding's layouts against C's: every size, alignment and")
    w("// field offset here was printed by a C program compiled against vulkan.h.")
    w("")
    w("module vulkan_layout_test;")
    w("")
    w("import {Expect} from support::expect;")
    w("import ultragui_vulkan::sys::vulkan;")
    w("")
    w("pub fn main(): i32 {")
    w("  var t = Expect();")
    for name, stype, decls, kind in structs:
        q = "ultragui_vulkan::sys::vulkan::" + name
        if name in sizes:
            sz, al = sizes[name]
            w('  t.eq_u32(@size_of(%s) as u32, %d, "sizeof(%s)");' % (q, sz, name))
            w('  t.eq_u32(@align_of(%s) as u32, %d, "alignof(%s)");' % (q, al, name))
        named = decls if kind in ("whole", "prefix") else (decls[:-1] if kind == "opaque_tail" else [])
        for ctype, fname, arr in named:
            w('  t.eq_u32(@offset_of(%s, %s) as u32, %d, "offsetof(%s, %s)");'
              % (q, field_name(fname), offsets[(name, fname)], name, fname))
    w("  return t.finish();")
    w("}")
    with open(os.path.join(PKG, "tests", "vulkan_layout_test.aki"), "w") as f:
        f.write("\n".join(t) + "\n")
    print("wrote src/sys/vulkan.aki and tests/vulkan_layout_test.aki")


if __name__ == "__main__":
    main()
