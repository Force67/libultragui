#ifndef SRC_RHI_VULKAN_VULKAN_RHI_H_
#define SRC_RHI_VULKAN_VULKAN_RHI_H_

#include <ultragui/platform/platform.h>
#include <ultragui/rhi/rhi.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

namespace ugui {

class VulkanRHI : public RHI {
public:
    bool Init(const RHIConfig& config) override;
    void Shutdown() override;
    bool BeginFrame(Color clear_color) override;
    void EndFrame() override;
    void SetScissor(Rect rect) override;
    void ResetScissor() override;
    void DrawTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                        u32 index_count, RHITextureHandle texture) override;
    void DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                             u32 index_count, RHITextureHandle atlas_texture) override;
    RHITextureHandle CreateTexture(u32 width, u32 height, RHIFormat format,
                                    const void* pixels,
                                    RHIFilter filter = RHIFilter::kLinear) override;
    void UpdateTexture(RHITextureHandle handle, const void* pixels) override;
    void DestroyTexture(RHITextureHandle handle) override;
    bool AcquireFrame() override;
    RHITextureHandle CreateRenderTarget(u32 width, u32 height) override;
    void DestroyRenderTarget(RHITextureHandle handle) override;
    bool BeginOffscreen(RHITextureHandle target, Color clear_color) override;
    void EndOffscreen(RHITextureHandle target) override;
    Vec2 display_size() const override;
    f32 dpi_scale() const override;

private:
    bool create_instance();
    bool create_surface();
    bool pick_physical_device();
    bool create_device();
    bool create_swapchain();
    void cleanup_swapchain();
    bool recreate_swapchain();
    bool create_render_pass();
    bool create_descriptor_layout();
    bool create_pipeline();
    bool create_text_pipeline();
    bool create_framebuffers();
    bool create_command_resources();
    bool create_sync_objects();
    bool create_default_resources();
    VkShaderModule load_shader(const char* filename);
    u32 find_memory_type(u32 type_filter, VkMemoryPropertyFlags properties);
    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
    void ensure_vertex_capacity(u32 vertex_count);
    void ensure_index_capacity(u32 index_count);
    void ensure_text_vertex_capacity(u32 vertex_count);
    void ensure_text_index_capacity(u32 index_count);

    GLFWwindow* window_ = nullptr;
    std::string shader_dir_;
    f32 dpi_scale_ = 1.0f;

    // Instance & device
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    u32 graphics_family_ = 0;
    u32 present_family_ = 0;
    bool validation_enabled_ = false;

    // Swapchain
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent_ = {};
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_views_;
    std::vector<VkFramebuffer> framebuffers_;

    // Pipeline
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_set_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipeline text_pipeline_ = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;

    // Per-frame data (one per in-flight frame)
    static constexpr u32 MAX_FRAMES = 2;
    struct FrameData {
        VkCommandPool cmd_pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
        VkSemaphore image_available = VK_NULL_HANDLE;
        VkFence in_flight = VK_NULL_HANDLE;
        // Quad pipeline buffers
        VkBuffer vertex_buffer = VK_NULL_HANDLE;
        VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
        u32 vertex_capacity = 0;
        u32 vertex_write_pos = 0; // append position (vertex count)
        VkBuffer index_buffer = VK_NULL_HANDLE;
        VkDeviceMemory index_memory = VK_NULL_HANDLE;
        u32 index_capacity = 0;
        u32 index_write_pos = 0; // append position (index count)
        // Text pipeline buffers (separate to avoid overwrite conflicts)
        VkBuffer text_vertex_buffer = VK_NULL_HANDLE;
        VkDeviceMemory text_vertex_memory = VK_NULL_HANDLE;
        u32 text_vertex_capacity = 0;
        u32 text_vertex_write_pos = 0;
        VkBuffer text_index_buffer = VK_NULL_HANDLE;
        VkDeviceMemory text_index_memory = VK_NULL_HANDLE;
        u32 text_index_capacity = 0;
        u32 text_index_write_pos = 0;
    };
    FrameData frames_[MAX_FRAMES];
    u32 current_frame_ = 0;
    u32 image_index_ = 0;
    bool framebuffer_resized_ = false;

    // Per-swapchain-image semaphores for presentation.
    // Indexed by image_index_ to avoid reuse while presentation engine holds them.
    static constexpr u32 MAX_SWAPCHAIN_IMAGES = 8;
    VkSemaphore render_finished_[MAX_SWAPCHAIN_IMAGES] = {};

    // Default resources
    VkSampler default_sampler_ = VK_NULL_HANDLE;  // LINEAR
    VkSampler nearest_sampler_ = VK_NULL_HANDLE; // NEAREST (for text atlases)

    bool create_offscreen_render_pass();

    struct TextureSlot {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet descriptor = VK_NULL_HANDLE;
        u32 width = 0;
        u32 height = 0;
        u32 pixel_size = 0; // bytes per pixel
        bool in_use = false;
        bool is_render_target = false;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
    };
    static constexpr u32 MAX_TEXTURES = 256;
    TextureSlot textures_[MAX_TEXTURES] = {};
    RHITextureHandle white_texture_ = kInvalidTexture;

    // Offscreen rendering state
    VkRenderPass offscreen_render_pass_ = VK_NULL_HANDLE;
    bool frame_acquired_ = false;
    RHITextureHandle active_offscreen_target_ = kInvalidTexture;
    Vec2 offscreen_display_size_ = {};

    // Vertex dedup: avoid re-uploading the same data within a Renderer2D frame
    const Vertex2D* last_quad_verts_ = nullptr;
    u32 last_quad_vert_count_ = 0;
    VkDeviceSize last_quad_vb_offset_ = 0;
    const Vertex2D* last_text_verts_ = nullptr;
    u32 last_text_vert_count_ = 0;
    VkDeviceSize last_text_vb_offset_ = 0;
};

} // namespace ugui

#endif  // SRC_RHI_VULKAN_VULKAN_RHI_H_
