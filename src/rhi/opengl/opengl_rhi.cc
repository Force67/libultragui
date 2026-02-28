#include <ultragui/rhi/rhi.h>
#include <ultragui/platform/platform.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// GL 3.3 core types and constants from Khronos header.
// GL_GLEXT_PROTOTYPES is intentionally NOT defined - we load all extension
// and core-profile function pointers manually via glfwGetProcAddress so
// that the binary does not depend on the system libGL exporting them.
#include <GL/glcorearb.h>

#include <dlfcn.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ugui {

// ---------------------------------------------------------------------------
// GL function pointer typedefs and globals
// ---------------------------------------------------------------------------

#define GL_FUNC(ret, name, ...) \
    typedef ret(APIENTRY* PFN_##name)(__VA_ARGS__); \
    static PFN_##name name = nullptr;

GL_FUNC(void, glGenVertexArrays, GLsizei, GLuint*)
GL_FUNC(void, glBindVertexArray, GLuint)
GL_FUNC(void, glDeleteVertexArrays, GLsizei, const GLuint*)
GL_FUNC(void, glGenBuffers, GLsizei, GLuint*)
GL_FUNC(void, glBindBuffer, GLenum, GLuint)
GL_FUNC(void, glDeleteBuffers, GLsizei, const GLuint*)
GL_FUNC(void, glBufferData, GLenum, GLsizeiptr, const void*, GLenum)
GL_FUNC(void, glBufferSubData, GLenum, GLintptr, GLsizeiptr, const void*)
GL_FUNC(void, glGenTextures, GLsizei, GLuint*)
GL_FUNC(void, glBindTexture, GLenum, GLuint)
GL_FUNC(void, glDeleteTextures, GLsizei, const GLuint*)
GL_FUNC(void, glTexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)
GL_FUNC(void, glTexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)
GL_FUNC(void, glTexParameteri, GLenum, GLenum, GLint)
GL_FUNC(void, glGenFramebuffers, GLsizei, GLuint*)
GL_FUNC(void, glBindFramebuffer, GLenum, GLuint)
GL_FUNC(void, glDeleteFramebuffers, GLsizei, const GLuint*)
GL_FUNC(void, glFramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint)
GL_FUNC(GLenum, glCheckFramebufferStatus, GLenum)
GL_FUNC(GLuint, glCreateShader, GLenum)
GL_FUNC(void, glShaderSource, GLuint, GLsizei, const GLchar* const*, const GLint*)
GL_FUNC(void, glCompileShader, GLuint)
GL_FUNC(void, glGetShaderiv, GLuint, GLenum, GLint*)
GL_FUNC(void, glGetShaderInfoLog, GLuint, GLsizei, GLsizei*, GLchar*)
GL_FUNC(void, glDeleteShader, GLuint)
GL_FUNC(GLuint, glCreateProgram, void)
GL_FUNC(void, glAttachShader, GLuint, GLuint)
GL_FUNC(void, glLinkProgram, GLuint)
GL_FUNC(void, glUseProgram, GLuint)
GL_FUNC(void, glDeleteProgram, GLuint)
GL_FUNC(void, glGetProgramiv, GLuint, GLenum, GLint*)
GL_FUNC(void, glGetProgramInfoLog, GLuint, GLsizei, GLsizei*, GLchar*)
GL_FUNC(GLint, glGetUniformLocation, GLuint, const GLchar*)
GL_FUNC(void, glUniform1i, GLint, GLint)
GL_FUNC(void, glUniform2f, GLint, GLfloat, GLfloat)
GL_FUNC(void, glEnableVertexAttribArray, GLuint)
GL_FUNC(void, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)
GL_FUNC(void, glVertexAttribIPointer, GLuint, GLint, GLenum, GLsizei, const void*)
GL_FUNC(void, glViewport, GLint, GLint, GLsizei, GLsizei)
GL_FUNC(void, glScissor, GLint, GLint, GLsizei, GLsizei)
GL_FUNC(void, glEnable, GLenum)
GL_FUNC(void, glDisable, GLenum)
GL_FUNC(void, glBlendFunc, GLenum, GLenum)
GL_FUNC(void, glBlendFuncSeparate, GLenum, GLenum, GLenum, GLenum)
GL_FUNC(void, glBlendEquation, GLenum)
GL_FUNC(void, glClearColor, GLfloat, GLfloat, GLfloat, GLfloat)
GL_FUNC(void, glClear, GLbitfield)
GL_FUNC(void, glDrawElements, GLenum, GLsizei, GLenum, const void*)
GL_FUNC(void, glDrawArrays, GLenum, GLint, GLsizei)
GL_FUNC(void, glActiveTexture, GLenum)
GL_FUNC(void, glPixelStorei, GLenum, GLint)
GL_FUNC(const GLubyte*, glGetString, GLenum)
GL_FUNC(const GLubyte*, glGetStringi, GLenum, GLuint)
GL_FUNC(void, glGetIntegerv, GLenum, GLint*)

#undef GL_FUNC

static bool load_gl_functions() {
    // glfwGetProcAddress may return NULL for legacy GL 1.x functions on some
    // drivers. Fall back to dlsym from libGL.so for those.
    static void* libgl = nullptr;
    if (!libgl) libgl = dlopen("libGL.so.1", RTLD_LAZY);
    auto load = [](const char* n) -> void* {
        void* p = (void*)glfwGetProcAddress(n);
        if (!p && libgl) p = dlsym(libgl, n);
        return p;
    };
    #define GL_LOAD(name) \
        name = reinterpret_cast<PFN_##name>(load(#name)); \
        if (!name) { std::fprintf(stderr, "ultragui: failed to load GL function '%s'\n", #name); return false; }

    GL_LOAD(glGenVertexArrays)
    GL_LOAD(glBindVertexArray)
    GL_LOAD(glDeleteVertexArrays)
    GL_LOAD(glGenBuffers)
    GL_LOAD(glBindBuffer)
    GL_LOAD(glDeleteBuffers)
    GL_LOAD(glBufferData)
    GL_LOAD(glBufferSubData)
    GL_LOAD(glGenTextures)
    GL_LOAD(glBindTexture)
    GL_LOAD(glDeleteTextures)
    GL_LOAD(glTexImage2D)
    GL_LOAD(glTexSubImage2D)
    GL_LOAD(glTexParameteri)
    GL_LOAD(glGenFramebuffers)
    GL_LOAD(glBindFramebuffer)
    GL_LOAD(glDeleteFramebuffers)
    GL_LOAD(glFramebufferTexture2D)
    GL_LOAD(glCheckFramebufferStatus)
    GL_LOAD(glCreateShader)
    GL_LOAD(glShaderSource)
    GL_LOAD(glCompileShader)
    GL_LOAD(glGetShaderiv)
    GL_LOAD(glGetShaderInfoLog)
    GL_LOAD(glDeleteShader)
    GL_LOAD(glCreateProgram)
    GL_LOAD(glAttachShader)
    GL_LOAD(glLinkProgram)
    GL_LOAD(glUseProgram)
    GL_LOAD(glDeleteProgram)
    GL_LOAD(glGetProgramiv)
    GL_LOAD(glGetProgramInfoLog)
    GL_LOAD(glGetUniformLocation)
    GL_LOAD(glUniform1i)
    GL_LOAD(glUniform2f)
    GL_LOAD(glEnableVertexAttribArray)
    GL_LOAD(glVertexAttribPointer)
    GL_LOAD(glVertexAttribIPointer)
    GL_LOAD(glViewport)
    GL_LOAD(glScissor)
    GL_LOAD(glEnable)
    GL_LOAD(glDisable)
    GL_LOAD(glBlendFunc)
    GL_LOAD(glBlendFuncSeparate)
    GL_LOAD(glBlendEquation)
    GL_LOAD(glClearColor)
    GL_LOAD(glClear)
    GL_LOAD(glDrawElements)
    GL_LOAD(glDrawArrays)
    GL_LOAD(glActiveTexture)
    GL_LOAD(glPixelStorei)
    GL_LOAD(glGetString)
    GL_LOAD(glGetStringi)
    GL_LOAD(glGetIntegerv)

    #undef GL_LOAD
    return true;
}

// ---------------------------------------------------------------------------
// Embedded GLSL shaders (GL 3.3 core, ported from Vulkan GLSL)
// ---------------------------------------------------------------------------

static const char* const kQuadVertSrc = R"glsl(
#version 330 core

uniform vec2 u_scale;
uniform vec2 u_translate;

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in uint in_color;
layout(location = 3) in uint in_color2;
layout(location = 4) in uint in_corner_radii;
layout(location = 5) in float in_softness;
layout(location = 6) in vec2 in_half_size;
layout(location = 7) in float in_border_width;
layout(location = 8) in uint in_border_color;

out vec2 frag_uv;
out vec4 frag_color;
out vec4 frag_color2;
out vec4 frag_corner_radii;
out float frag_softness;
out vec2 frag_half_size;
out float frag_border_width;
out vec4 frag_border_color;

vec3 srgb_to_linear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

vec4 unpack_color(uint c) {
    vec4 col = vec4(
        float(c & 0xFFu) / 255.0,
        float((c >> 8) & 0xFFu) / 255.0,
        float((c >> 16) & 0xFFu) / 255.0,
        float((c >> 24) & 0xFFu) / 255.0
    );
    col.rgb = srgb_to_linear(col.rgb);
    return col;
}

void main() {
    gl_Position = vec4(in_pos * u_scale + u_translate, 0.0, 1.0);

    frag_uv = in_uv;
    frag_color = unpack_color(in_color);
    frag_color2 = unpack_color(in_color2);
    frag_corner_radii = vec4(
        float(in_corner_radii & 0xFFu),
        float((in_corner_radii >> 8) & 0xFFu),
        float((in_corner_radii >> 16) & 0xFFu),
        float((in_corner_radii >> 24) & 0xFFu)
    );
    frag_softness = in_softness;
    frag_half_size = in_half_size;
    frag_border_width = in_border_width;
    frag_border_color = unpack_color(in_border_color);
}
)glsl";

static const char* const kQuadFragSrc = R"glsl(
#version 330 core

in vec2 frag_uv;
in vec4 frag_color;
in vec4 frag_color2;
in vec4 frag_corner_radii;
in float frag_softness;
in vec2 frag_half_size;
in float frag_border_width;
in vec4 frag_border_color;

uniform sampler2D tex_sampler;

out vec4 out_color;

vec3 srgb_to_linear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

float sdf_rounded_rect_4(vec2 p, vec2 b, vec4 radii) {
    float radius = (p.x > 0.0) ? ((p.y > 0.0) ? radii.z : radii.y)
                                : ((p.y > 0.0) ? radii.w : radii.x);
    vec2 q = abs(p) - b + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

void main() {
    vec4 tex_color = texture(tex_sampler, frag_uv);
    tex_color.rgb = srgb_to_linear(tex_color.rgb);

    vec4 base_color = mix(frag_color, frag_color2, frag_uv.y);
    vec4 color = base_color * tex_color;

    if (frag_half_size.x > 0.0 && frag_half_size.y > 0.0) {
        vec2 local = (frag_uv * 2.0 - 1.0) * frag_half_size;
        float d = sdf_rounded_rect_4(local, frag_half_size, frag_corner_radii);

        float soft = abs(frag_softness);
        float aa = max(fwidth(d) * 0.75, soft);
        float alpha;
        if (frag_softness < 0.0) {
            alpha = smoothstep(-aa, 0.0, d);
        } else {
            alpha = 1.0 - smoothstep(-aa, aa, d);
        }
        color.a *= alpha;

        if (frag_border_width > 0.0 && frag_border_color.a > 0.0) {
            vec2 inner_half = frag_half_size - vec2(frag_border_width);
            vec4 inner_radii = max(frag_corner_radii - vec4(frag_border_width), vec4(0.0));
            float d_inner = sdf_rounded_rect_4(local, inner_half, inner_radii);
            float inner_aa = fwidth(d_inner) * 0.75;
            float inner_alpha = 1.0 - smoothstep(-inner_aa, inner_aa, d_inner);

            float border_mask = alpha * (1.0 - inner_alpha);
            vec4 border_col = frag_border_color;
            border_col.a *= border_mask;

            vec4 fill = color;
            fill.a *= inner_alpha;

            color = fill + border_col * (1.0 - fill.a);
            color.a = fill.a + border_col.a * (1.0 - fill.a);
        }
    }

    out_color = color;
}
)glsl";

static const char* const kTextVertSrc = R"glsl(
#version 330 core

uniform vec2 u_scale;
uniform vec2 u_translate;

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in uint in_color;
layout(location = 3) in uint in_color2;
layout(location = 4) in uint in_corner_radii;
layout(location = 5) in float in_softness;
layout(location = 6) in vec2 in_half_size;
layout(location = 7) in float in_border_width;
layout(location = 8) in uint in_border_color;

out vec2 frag_uv;
out vec4 frag_color;

vec3 srgb_to_linear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

void main() {
    gl_Position = vec4(in_pos * u_scale + u_translate, 0.0, 1.0);
    frag_uv = in_uv;

    vec4 col = vec4(
        float(in_color & 0xFFu) / 255.0,
        float((in_color >> 8) & 0xFFu) / 255.0,
        float((in_color >> 16) & 0xFFu) / 255.0,
        float((in_color >> 24) & 0xFFu) / 255.0);
    col.rgb = srgb_to_linear(col.rgb);
    frag_color = col;
}
)glsl";

static const char* const kTextFragSrc = R"glsl(
#version 330 core

in vec2 frag_uv;
in vec4 frag_color;

uniform sampler2D tex_sampler;

out vec4 out_color;

void main() {
    float alpha = texture(tex_sampler, frag_uv).r;
    out_color = vec4(frag_color.rgb, frag_color.a * alpha);
}
)glsl";

static const char* const kVideoVertSrc = R"glsl(
#version 330 core

out vec2 frag_uv;

void main() {
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    frag_uv = uvs[gl_VertexID];
}
)glsl";

static const char* const kVideoFragSrc = R"glsl(
#version 330 core

uniform sampler2D tex_y;
uniform sampler2D tex_cb;
uniform sampler2D tex_cr;

in vec2 frag_uv;
out vec4 out_color;

void main() {
    float y  = texture(tex_y,  frag_uv).r;
    float cb = texture(tex_cb, frag_uv).r;
    float cr = texture(tex_cr, frag_uv).r;

    vec4 ycbcr = vec4(y, cb, cr, 1.0);
    float r = dot(ycbcr, vec4(1.16438,  0.00000,  1.59603, -0.87079));
    float g = dot(ycbcr, vec4(1.16438, -0.39176, -0.81297,  0.52959));
    float b = dot(ycbcr, vec4(1.16438,  2.01723,  0.00000, -1.08139));

    out_color = vec4(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// Shader compilation helpers
// ---------------------------------------------------------------------------

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "ultragui: shader compile error:\n%s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint compile_program(const char* vs_src, const char* fs_src) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    if (!vs) return 0;

    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "ultragui: program link error:\n%s\n", log);
        glDeleteProgram(prog);
        prog = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ---------------------------------------------------------------------------
// sRGB helper - glClearColor bypasses GL_FRAMEBUFFER_SRGB conversion,
// so we manually apply the sRGB transfer function to the clear color.
// ---------------------------------------------------------------------------

static f32 linear_to_srgb(f32 c) {
    if (c <= 0.0031308f)
        return c * 12.92f;
    return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

// ---------------------------------------------------------------------------
// RHI::Impl
// ---------------------------------------------------------------------------

struct RHI::Impl {
    GLFWwindow* window_ = nullptr;
    f32 dpi_scale_ = 1.0f;
    bool vsync_ = true;

    // Shader programs
    GLuint quad_program_ = 0;
    GLuint text_program_ = 0;
    GLuint video_program_ = 0; // lazy-created

    // Uniform locations - quad program
    GLint quad_u_scale_ = -1;
    GLint quad_u_translate_ = -1;
    GLint quad_u_tex_ = -1;

    // Uniform locations - text program
    GLint text_u_scale_ = -1;
    GLint text_u_translate_ = -1;
    GLint text_u_tex_ = -1;

    // Uniform locations - video program
    GLint video_u_y_ = -1;
    GLint video_u_cb_ = -1;
    GLint video_u_cr_ = -1;

    // Shared VAO (vertex format is identical for quad and text)
    GLuint vao_ = 0;

    // Video VAO (empty - vertices generated from gl_VertexID)
    GLuint video_vao_ = 0;

    // Quad pipeline dynamic vertex/index buffers
    GLuint vertex_buf_ = 0;
    u32 vertex_capacity_ = 0;
    u32 vertex_write_pos_ = 0;

    GLuint index_buf_ = 0;
    u32 index_capacity_ = 0;
    u32 index_write_pos_ = 0;

    // Text pipeline separate buffers
    GLuint text_vertex_buf_ = 0;
    u32 text_vertex_capacity_ = 0;
    u32 text_vertex_write_pos_ = 0;

    GLuint text_index_buf_ = 0;
    u32 text_index_capacity_ = 0;
    u32 text_index_write_pos_ = 0;

    // Vertex dedup tracking (same pattern as Vulkan backend)
    const Vertex2D* last_quad_verts_ = nullptr;
    u32 last_quad_vert_count_ = 0;
    u32 last_quad_vb_offset_ = 0; // byte offset

    const Vertex2D* last_text_verts_ = nullptr;
    u32 last_text_vert_count_ = 0;
    u32 last_text_vb_offset_ = 0;

    // Texture slots
    static constexpr u32 MAX_TEXTURES = 256;

    struct TextureSlot {
        GLuint texture = 0;
        GLuint fbo = 0; // only for render targets
        u32 width = 0;
        u32 height = 0;
        u32 pixel_size = 0;
        bool in_use = false;
        bool is_render_target = false;
        GLenum internal_format = 0;
        GLenum data_format = 0;
        GLenum data_type = 0;
    };

    TextureSlot textures_[MAX_TEXTURES] = {};
    RHITextureHandle white_texture_ = kInvalidTexture;

    // Offscreen state
    RHITextureHandle active_offscreen_target_ = kInvalidTexture;
    Vec2 offscreen_display_size_ = {};
    bool frame_active_ = false;

    u32 swapchain_width_ = 0;
    u32 swapchain_height_ = 0;

    // Current projection (cached for program switches)
    f32 proj_scale_[2] = {};
    f32 proj_translate_[2] = {};

    // Methods
    bool Init(const RHIConfig& config);
    void Shutdown();

    bool AcquireFrame();
    bool BeginFrame(Color clear_color);
    void EndFrame();

    void SetScissor(Rect rect);
    void ResetScissor();

    void DrawTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                       u32 index_count, RHITextureHandle texture);
    void DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                           u32 index_count, RHITextureHandle atlas_texture);

    RHITextureHandle CreateTexture(u32 width, u32 height, RHIFormat format,
                                   const void* pixels, RHIFilter filter);
    void UpdateTexture(RHITextureHandle handle, const void* pixels);
    void DestroyTexture(RHITextureHandle handle);

    RHITextureHandle CreateRenderTarget(u32 width, u32 height);
    void DestroyRenderTarget(RHITextureHandle handle);

    bool BeginOffscreen(RHITextureHandle target, Color clear_color);
    void EndOffscreen(RHITextureHandle target);

    void ConvertVideoFrame(RHITextureHandle target, RHITextureHandle y,
                           RHITextureHandle cb, RHITextureHandle cr);

    Vec2 display_size() const;
    f32 dpi_scale() const;

    // Internal helpers
    void ensure_vertex_capacity(u32 needed);
    void ensure_index_capacity(u32 needed);
    void ensure_text_vertex_capacity(u32 needed);
    void ensure_text_index_capacity(u32 needed);
    bool ensure_video_program();

    void bind_vertex_format(GLuint vbo, GLuint ibo);
    void set_projection(GLuint program, f32 win_w, f32 win_h);
};

// ---------------------------------------------------------------------------
// Static instance for framebuffer resize callback
// ---------------------------------------------------------------------------

static RHI::Impl* s_rhi_instance = nullptr;

// ---------------------------------------------------------------------------
// Vertex format binding
// ---------------------------------------------------------------------------

void RHI::Impl::bind_vertex_format(GLuint vbo, GLuint ibo) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // 0: pos (2xfloat, offset 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D),
                          reinterpret_cast<void*>(offsetof(Vertex2D, pos)));
    // 1: uv (2xfloat, offset 8)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D),
                          reinterpret_cast<void*>(offsetof(Vertex2D, uv)));
    // 2: color (1xuint, offset 16) - integer attribute
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(Vertex2D),
                           reinterpret_cast<void*>(offsetof(Vertex2D, color)));
    // 3: color2 (1xuint, offset 20)
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(Vertex2D),
                           reinterpret_cast<void*>(offsetof(Vertex2D, color2)));
    // 4: corner_radii (1xuint, offset 24)
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(Vertex2D),
                           reinterpret_cast<void*>(offsetof(Vertex2D, corner_radii)));
    // 5: softness (1xfloat, offset 28)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex2D),
                          reinterpret_cast<void*>(offsetof(Vertex2D, softness)));
    // 6: half_size (2xfloat, offset 32)
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D),
                          reinterpret_cast<void*>(offsetof(Vertex2D, half_size)));
    // 7: border_width (1xfloat, offset 40)
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex2D),
                          reinterpret_cast<void*>(offsetof(Vertex2D, border_width)));
    // 8: border_color (1xuint, offset 44)
    glEnableVertexAttribArray(8);
    glVertexAttribIPointer(8, 1, GL_UNSIGNED_INT, sizeof(Vertex2D),
                           reinterpret_cast<void*>(offsetof(Vertex2D, border_color)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
}

// ---------------------------------------------------------------------------
// Projection helper
// ---------------------------------------------------------------------------

void RHI::Impl::set_projection(GLuint program, f32 win_w, f32 win_h) {
    // OpenGL NDC: Y-up. We want screen coordinates (Y-down), so flip Y.
    proj_scale_[0] = 2.0f / win_w;
    proj_scale_[1] = -2.0f / win_h;
    proj_translate_[0] = -1.0f;
    proj_translate_[1] = 1.0f;

    GLint u_scale = (program == quad_program_) ? quad_u_scale_ : text_u_scale_;
    GLint u_translate = (program == quad_program_) ? quad_u_translate_ : text_u_translate_;

    glUniform2f(u_scale, proj_scale_[0], proj_scale_[1]);
    glUniform2f(u_translate, proj_translate_[0], proj_translate_[1]);
}

// ---------------------------------------------------------------------------
// Buffer capacity helpers
// ---------------------------------------------------------------------------

void RHI::Impl::ensure_vertex_capacity(u32 needed) {
    if (vertex_capacity_ >= needed)
        return;

    u32 new_cap = std::max(needed, vertex_capacity_ * 2);
    new_cap = std::max(new_cap, 16384u);

    if (!vertex_buf_)
        glGenBuffers(1, &vertex_buf_);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buf_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(new_cap * sizeof(Vertex2D)),
                 nullptr, GL_DYNAMIC_DRAW);
    vertex_capacity_ = new_cap;
}

void RHI::Impl::ensure_index_capacity(u32 needed) {
    if (index_capacity_ >= needed)
        return;

    u32 new_cap = std::max(needed, index_capacity_ * 2);
    new_cap = std::max(new_cap, 32768u);

    if (!index_buf_)
        glGenBuffers(1, &index_buf_);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(new_cap * sizeof(u32)),
                 nullptr, GL_DYNAMIC_DRAW);
    index_capacity_ = new_cap;
}

void RHI::Impl::ensure_text_vertex_capacity(u32 needed) {
    if (text_vertex_capacity_ >= needed)
        return;

    u32 new_cap = std::max(needed, text_vertex_capacity_ * 2);
    new_cap = std::max(new_cap, 16384u);

    if (!text_vertex_buf_)
        glGenBuffers(1, &text_vertex_buf_);

    glBindBuffer(GL_ARRAY_BUFFER, text_vertex_buf_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(new_cap * sizeof(Vertex2D)),
                 nullptr, GL_DYNAMIC_DRAW);
    text_vertex_capacity_ = new_cap;
}

void RHI::Impl::ensure_text_index_capacity(u32 needed) {
    if (text_index_capacity_ >= needed)
        return;

    u32 new_cap = std::max(needed, text_index_capacity_ * 2);
    new_cap = std::max(new_cap, 32768u);

    if (!text_index_buf_)
        glGenBuffers(1, &text_index_buf_);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_index_buf_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(new_cap * sizeof(u32)),
                 nullptr, GL_DYNAMIC_DRAW);
    text_index_capacity_ = new_cap;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

bool RHI::Impl::Init(const RHIConfig& config) {
    window_ = static_cast<GLFWwindow*>(config.platform->native_handle());
    vsync_ = config.vsync;

    glfwMakeContextCurrent(window_);

    if (!load_gl_functions())
        return false;

    // Print GL version
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::printf("ultragui: OpenGL %s (%s)\n", version ? version : "unknown",
                renderer ? renderer : "unknown");

    // Compute DPI scale
    {
        int fw, fh, ww, wh;
        glfwGetFramebufferSize(window_, &fw, &fh);
        glfwGetWindowSize(window_, &ww, &wh);
        dpi_scale_ = (ww > 0) ? static_cast<f32>(fw) / static_cast<f32>(ww) : 1.0f;
    }

    // Framebuffer resize callback
    s_rhi_instance = this;
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow*, int, int) {
        if (!s_rhi_instance)
            return;
        int fw2, fh2, ww2, wh2;
        glfwGetFramebufferSize(s_rhi_instance->window_, &fw2, &fh2);
        glfwGetWindowSize(s_rhi_instance->window_, &ww2, &wh2);
        s_rhi_instance->dpi_scale_ =
            (ww2 > 0) ? static_cast<f32>(fw2) / static_cast<f32>(ww2) : 1.0f;
    });

    // GL state
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_SCISSOR_TEST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Compile shader programs
    quad_program_ = compile_program(kQuadVertSrc, kQuadFragSrc);
    if (!quad_program_) return false;

    text_program_ = compile_program(kTextVertSrc, kTextFragSrc);
    if (!text_program_) return false;

    // Get uniform locations
    quad_u_scale_     = glGetUniformLocation(quad_program_, "u_scale");
    quad_u_translate_ = glGetUniformLocation(quad_program_, "u_translate");
    quad_u_tex_       = glGetUniformLocation(quad_program_, "tex_sampler");

    text_u_scale_     = glGetUniformLocation(text_program_, "u_scale");
    text_u_translate_ = glGetUniformLocation(text_program_, "u_translate");
    text_u_tex_       = glGetUniformLocation(text_program_, "tex_sampler");

    // Set texture unit 0 for both programs
    glUseProgram(quad_program_);
    glUniform1i(quad_u_tex_, 0);
    glUseProgram(text_program_);
    glUniform1i(text_u_tex_, 0);

    // Create VAO
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    // Create initial buffers
    glGenBuffers(1, &vertex_buf_);
    glGenBuffers(1, &index_buf_);
    glGenBuffers(1, &text_vertex_buf_);
    glGenBuffers(1, &text_index_buf_);

    // Allocate initial buffer storage
    vertex_capacity_ = 16384;
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buf_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertex_capacity_ * sizeof(Vertex2D)),
                 nullptr, GL_DYNAMIC_DRAW);

    index_capacity_ = 32768;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(index_capacity_ * sizeof(u32)),
                 nullptr, GL_DYNAMIC_DRAW);

    text_vertex_capacity_ = 16384;
    glBindBuffer(GL_ARRAY_BUFFER, text_vertex_buf_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(text_vertex_capacity_ * sizeof(Vertex2D)),
                 nullptr, GL_DYNAMIC_DRAW);

    text_index_capacity_ = 32768;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_index_buf_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(text_index_capacity_ * sizeof(u32)),
                 nullptr, GL_DYNAMIC_DRAW);

    // Create white 1x1 fallback texture
    u32 white_pixel = 0xFFFFFFFF;
    white_texture_ = CreateTexture(1, 1, RHIFormat::kRgba8Unorm, &white_pixel, RHIFilter::kNearest);
    if (white_texture_ == kInvalidTexture) return false;

    // Swap interval
    glfwSwapInterval(vsync_ ? 1 : 0);

    // Get initial framebuffer size
    {
        int fw, fh;
        glfwGetFramebufferSize(window_, &fw, &fh);
        swapchain_width_ = static_cast<u32>(fw);
        swapchain_height_ = static_cast<u32>(fh);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void RHI::Impl::Shutdown() {
    s_rhi_instance = nullptr;

    // Destroy textures
    for (u32 i = 0; i < MAX_TEXTURES; ++i) {
        if (textures_[i].in_use)
            DestroyTexture(i);
    }

    if (vertex_buf_) glDeleteBuffers(1, &vertex_buf_);
    if (index_buf_) glDeleteBuffers(1, &index_buf_);
    if (text_vertex_buf_) glDeleteBuffers(1, &text_vertex_buf_);
    if (text_index_buf_) glDeleteBuffers(1, &text_index_buf_);

    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (video_vao_) glDeleteVertexArrays(1, &video_vao_);

    if (quad_program_) glDeleteProgram(quad_program_);
    if (text_program_) glDeleteProgram(text_program_);
    if (video_program_) glDeleteProgram(video_program_);
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------

bool RHI::Impl::AcquireFrame() {
    if (frame_active_)
        return true;

    // Reset write positions
    vertex_write_pos_ = 0;
    index_write_pos_ = 0;
    text_vertex_write_pos_ = 0;
    text_index_write_pos_ = 0;

    // Reset dedup tracking
    last_quad_verts_ = nullptr;
    last_quad_vert_count_ = 0;
    last_text_verts_ = nullptr;
    last_text_vert_count_ = 0;

    frame_active_ = true;
    return true;
}

bool RHI::Impl::BeginFrame(Color clear_color) {
    if (!AcquireFrame())
        return false;

    // Update framebuffer size
    {
        int fw, fh;
        glfwGetFramebufferSize(window_, &fw, &fh);
        swapchain_width_ = static_cast<u32>(fw);
        swapchain_height_ = static_cast<u32>(fh);
    }

    // Bind the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Set viewport and scissor to full framebuffer
    glViewport(0, 0, static_cast<GLsizei>(swapchain_width_),
               static_cast<GLsizei>(swapchain_height_));
    glScissor(0, 0, static_cast<GLsizei>(swapchain_width_),
              static_cast<GLsizei>(swapchain_height_));

    // Clear - glClearColor is NOT affected by GL_FRAMEBUFFER_SRGB, so we
    // must manually encode the linear clear color into sRGB for correct output.
    glClearColor(linear_to_srgb(clear_color.r),
                 linear_to_srgb(clear_color.g),
                 linear_to_srgb(clear_color.b),
                 clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT);

    // Activate quad program and set projection
    glUseProgram(quad_program_);
    glBindVertexArray(vao_);

    f32 win_w = static_cast<f32>(swapchain_width_) / dpi_scale_;
    f32 win_h = static_cast<f32>(swapchain_height_) / dpi_scale_;
    set_projection(quad_program_, win_w, win_h);

    return true;
}

void RHI::Impl::EndFrame() {
    glfwSwapBuffers(window_);
    frame_active_ = false;
}

// ---------------------------------------------------------------------------
// Scissor
// ---------------------------------------------------------------------------

void RHI::Impl::SetScissor(Rect rect) {
    f32 scale = (active_offscreen_target_ != kInvalidTexture) ? 1.0f : dpi_scale_;
    f32 fb_h = (active_offscreen_target_ != kInvalidTexture)
                   ? offscreen_display_size_.y
                   : static_cast<f32>(swapchain_height_);

    // OpenGL scissor origin is bottom-left; UI coords have origin top-left.
    GLint x = static_cast<GLint>(rect.x * scale);
    GLint w = static_cast<GLint>(rect.w * scale);
    GLint h = static_cast<GLint>(rect.h * scale);
    GLint y = static_cast<GLint>(fb_h - (rect.y * scale + static_cast<f32>(h)));

    glScissor(x, y, w, h);
}

void RHI::Impl::ResetScissor() {
    if (active_offscreen_target_ != kInvalidTexture) {
        auto& slot = textures_[active_offscreen_target_];
        glScissor(0, 0, static_cast<GLsizei>(slot.width), static_cast<GLsizei>(slot.height));
    } else {
        glScissor(0, 0, static_cast<GLsizei>(swapchain_width_),
                  static_cast<GLsizei>(swapchain_height_));
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void RHI::Impl::DrawTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                               u32 index_count, RHITextureHandle texture) {
    if (vertex_count == 0 || index_count == 0)
        return;

    glUseProgram(quad_program_);
    glBindVertexArray(vao_);

    // Vertex dedup: Renderer2D passes the same vertex array for every batch
    // in a frame, with different index slices.
    u32 vb_byte_offset;
    if (vertices == last_quad_verts_ && vertex_count == last_quad_vert_count_) {
        vb_byte_offset = last_quad_vb_offset_;
    } else {
        ensure_vertex_capacity(vertex_write_pos_ + vertex_count);
        vb_byte_offset = vertex_write_pos_ * static_cast<u32>(sizeof(Vertex2D));

        glBindBuffer(GL_ARRAY_BUFFER, vertex_buf_);
        glBufferSubData(GL_ARRAY_BUFFER, vb_byte_offset,
                        static_cast<GLsizeiptr>(vertex_count * sizeof(Vertex2D)), vertices);

        last_quad_verts_ = vertices;
        last_quad_vert_count_ = vertex_count;
        last_quad_vb_offset_ = vb_byte_offset;
        vertex_write_pos_ += vertex_count;
    }

    // Indices are always different per batch - always append
    ensure_index_capacity(index_write_pos_ + index_count);
    u32 ib_byte_offset = index_write_pos_ * static_cast<u32>(sizeof(u32));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf_);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, ib_byte_offset,
                    static_cast<GLsizeiptr>(index_count * sizeof(u32)), indices);
    index_write_pos_ += index_count;

    // Bind texture
    RHITextureHandle tex = (texture != kInvalidTexture) ? texture : white_texture_;
    if (tex < MAX_TEXTURES && textures_[tex].in_use) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures_[tex].texture);
    }

    // Set up vertex format with the correct VBO offset
    bind_vertex_format(vertex_buf_, index_buf_);

    // Set projection uniforms (in case program was switched)
    glUniform2f(quad_u_scale_, proj_scale_[0], proj_scale_[1]);
    glUniform2f(quad_u_translate_, proj_translate_[0], proj_translate_[1]);

    // Draw
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count), GL_UNSIGNED_INT,
                   reinterpret_cast<void*>(static_cast<uintptr_t>(ib_byte_offset)));
}

void RHI::Impl::DrawTextTriangles(const Vertex2D* vertices, u32 vertex_count, const u32* indices,
                                    u32 index_count, RHITextureHandle atlas_texture) {
    if (vertex_count == 0 || index_count == 0)
        return;

    // Switch to text program
    glUseProgram(text_program_);
    glBindVertexArray(vao_);

    // Set projection on text program (use display_size for offscreen support)
    Vec2 ds = display_size();
    set_projection(text_program_, ds.x, ds.y);

    // Vertex dedup (same pattern)
    u32 vb_byte_offset;
    if (vertices == last_text_verts_ && vertex_count == last_text_vert_count_) {
        vb_byte_offset = last_text_vb_offset_;
    } else {
        ensure_text_vertex_capacity(text_vertex_write_pos_ + vertex_count);
        vb_byte_offset = text_vertex_write_pos_ * static_cast<u32>(sizeof(Vertex2D));

        glBindBuffer(GL_ARRAY_BUFFER, text_vertex_buf_);
        glBufferSubData(GL_ARRAY_BUFFER, vb_byte_offset,
                        static_cast<GLsizeiptr>(vertex_count * sizeof(Vertex2D)), vertices);

        last_text_verts_ = vertices;
        last_text_vert_count_ = vertex_count;
        last_text_vb_offset_ = vb_byte_offset;
        text_vertex_write_pos_ += vertex_count;
    }

    // Append indices
    ensure_text_index_capacity(text_index_write_pos_ + index_count);
    u32 ib_byte_offset = text_index_write_pos_ * static_cast<u32>(sizeof(u32));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, text_index_buf_);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, ib_byte_offset,
                    static_cast<GLsizeiptr>(index_count * sizeof(u32)), indices);
    text_index_write_pos_ += index_count;

    // Bind atlas texture
    if (atlas_texture < MAX_TEXTURES && textures_[atlas_texture].in_use) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures_[atlas_texture].texture);
    }

    // Set up vertex format with text buffers
    bind_vertex_format(text_vertex_buf_, text_index_buf_);

    // Draw
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count), GL_UNSIGNED_INT,
                   reinterpret_cast<void*>(static_cast<uintptr_t>(ib_byte_offset)));

    // Switch back to quad program
    glUseProgram(quad_program_);
    glUniform2f(quad_u_scale_, proj_scale_[0], proj_scale_[1]);
    glUniform2f(quad_u_translate_, proj_translate_[0], proj_translate_[1]);
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
    GLenum internal_format = GL_RGBA8;
    GLenum data_format = GL_RGBA;
    GLenum data_type = GL_UNSIGNED_BYTE;
    u32 pixel_size = 4;

    switch (format) {
    case RHIFormat::kRgba8Unorm:
        internal_format = GL_RGBA8;
        data_format = GL_RGBA;
        pixel_size = 4;
        break;
    case RHIFormat::kBgra8Unorm:
        internal_format = GL_RGBA8;
        data_format = GL_BGRA;
        pixel_size = 4;
        break;
    case RHIFormat::kR8Unorm:
        internal_format = GL_R8;
        data_format = GL_RED;
        pixel_size = 1;
        break;
    default:
        return kInvalidTexture;
    }

    glGenTextures(1, &slot.texture);
    glBindTexture(GL_TEXTURE_2D, slot.texture);

    // Set filtering
    GLint mag_filter = (filter == RHIFilter::kNearest) ? GL_NEAREST : GL_LINEAR;
    GLint min_filter = (filter == RHIFilter::kNearest) ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload pixel data
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal_format),
                 static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
                 data_format, data_type, pixels);

    slot.width = width;
    slot.height = height;
    slot.pixel_size = pixel_size;
    slot.internal_format = internal_format;
    slot.data_format = data_format;
    slot.data_type = data_type;
    slot.in_use = true;
    slot.is_render_target = false;

    return handle;
}

void RHI::Impl::UpdateTexture(RHITextureHandle handle, const void* pixels) {
    if (handle >= MAX_TEXTURES || !textures_[handle].in_use)
        return;

    auto& slot = textures_[handle];
    glBindTexture(GL_TEXTURE_2D, slot.texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    static_cast<GLsizei>(slot.width), static_cast<GLsizei>(slot.height),
                    slot.data_format, slot.data_type, pixels);
}

void RHI::Impl::DestroyTexture(RHITextureHandle handle) {
    if (handle >= MAX_TEXTURES || !textures_[handle].in_use)
        return;
    if (textures_[handle].is_render_target) {
        DestroyRenderTarget(handle);
        return;
    }
    auto& slot = textures_[handle];
    if (slot.texture) glDeleteTextures(1, &slot.texture);
    slot = {};
}

// ---------------------------------------------------------------------------
// Offscreen render targets
// ---------------------------------------------------------------------------

RHITextureHandle RHI::Impl::CreateRenderTarget(u32 width, u32 height) {
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

    // Create texture for the render target
    glGenTextures(1, &slot.texture);
    glBindTexture(GL_TEXTURE_2D, slot.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create FBO
    glGenFramebuffers(1, &slot.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, slot.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           slot.texture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "ultragui: framebuffer incomplete (0x%x)\n", status);
        glDeleteFramebuffers(1, &slot.fbo);
        glDeleteTextures(1, &slot.texture);
        slot = {};
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return kInvalidTexture;
    }

    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    slot.width = width;
    slot.height = height;
    slot.pixel_size = 4;
    slot.internal_format = GL_RGBA8;
    slot.data_format = GL_RGBA;
    slot.data_type = GL_UNSIGNED_BYTE;
    slot.in_use = true;
    slot.is_render_target = true;

    return handle;
}

void RHI::Impl::DestroyRenderTarget(RHITextureHandle handle) {
    if (handle >= MAX_TEXTURES || !textures_[handle].in_use || !textures_[handle].is_render_target)
        return;

    auto& slot = textures_[handle];
    if (slot.fbo) glDeleteFramebuffers(1, &slot.fbo);
    if (slot.texture) glDeleteTextures(1, &slot.texture);
    slot = {};
}

bool RHI::Impl::BeginOffscreen(RHITextureHandle target, Color clear_color) {
    if (target >= MAX_TEXTURES || !textures_[target].in_use || !textures_[target].is_render_target)
        return false;

    auto& slot = textures_[target];

    active_offscreen_target_ = target;
    offscreen_display_size_ = {static_cast<f32>(slot.width), static_cast<f32>(slot.height)};

    glBindFramebuffer(GL_FRAMEBUFFER, slot.fbo);
    glViewport(0, 0, static_cast<GLsizei>(slot.width), static_cast<GLsizei>(slot.height));
    glScissor(0, 0, static_cast<GLsizei>(slot.width), static_cast<GLsizei>(slot.height));

    glClearColor(linear_to_srgb(clear_color.r),
                 linear_to_srgb(clear_color.g),
                 linear_to_srgb(clear_color.b),
                 clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up quad program with offscreen projection
    glUseProgram(quad_program_);
    glBindVertexArray(vao_);
    set_projection(quad_program_, static_cast<f32>(slot.width), static_cast<f32>(slot.height));

    return true;
}

void RHI::Impl::EndOffscreen(RHITextureHandle target) {
    if (active_offscreen_target_ == kInvalidTexture)
        return;

    (void)target;
    active_offscreen_target_ = kInvalidTexture;

    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, static_cast<GLsizei>(swapchain_width_),
               static_cast<GLsizei>(swapchain_height_));
    glScissor(0, 0, static_cast<GLsizei>(swapchain_width_),
              static_cast<GLsizei>(swapchain_height_));

    // Restore screen projection
    f32 win_w = static_cast<f32>(swapchain_width_) / dpi_scale_;
    f32 win_h = static_cast<f32>(swapchain_height_) / dpi_scale_;
    glUseProgram(quad_program_);
    set_projection(quad_program_, win_w, win_h);
}

// ---------------------------------------------------------------------------
// Video pipeline (YCbCr -> RGBA)
// ---------------------------------------------------------------------------

bool RHI::Impl::ensure_video_program() {
    if (video_program_)
        return true;

    video_program_ = compile_program(kVideoVertSrc, kVideoFragSrc);
    if (!video_program_)
        return false;

    video_u_y_  = glGetUniformLocation(video_program_, "tex_y");
    video_u_cb_ = glGetUniformLocation(video_program_, "tex_cb");
    video_u_cr_ = glGetUniformLocation(video_program_, "tex_cr");

    // Set texture units
    glUseProgram(video_program_);
    glUniform1i(video_u_y_,  0);
    glUniform1i(video_u_cb_, 1);
    glUniform1i(video_u_cr_, 2);

    // Create empty VAO for video (no vertex attributes - gl_VertexID only)
    glGenVertexArrays(1, &video_vao_);

    return true;
}

void RHI::Impl::ConvertVideoFrame(RHITextureHandle target,
                                   RHITextureHandle y, RHITextureHandle cb,
                                   RHITextureHandle cr) {
    if (!ensure_video_program())
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

    // Bind the render target FBO
    glBindFramebuffer(GL_FRAMEBUFFER, slot.fbo);
    glViewport(0, 0, static_cast<GLsizei>(slot.width), static_cast<GLsizei>(slot.height));
    glScissor(0, 0, static_cast<GLsizei>(slot.width), static_cast<GLsizei>(slot.height));

    // Disable blending for opaque video
    glDisable(GL_BLEND);

    // Bind video program
    glUseProgram(video_program_);
    glBindVertexArray(video_vao_);

    // Bind Y, Cb, Cr textures to units 0, 1, 2
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[y].texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures_[cb].texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textures_[cr].texture);

    // Draw fullscreen triangle (3 vertices, no VBO)
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Restore state
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_BLEND);

    // Restore the quad program and default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, static_cast<GLsizei>(swapchain_width_),
               static_cast<GLsizei>(swapchain_height_));
    glScissor(0, 0, static_cast<GLsizei>(swapchain_width_),
              static_cast<GLsizei>(swapchain_height_));

    glUseProgram(quad_program_);
    glBindVertexArray(vao_);
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

Vec2 RHI::Impl::display_size() const {
    if (active_offscreen_target_ != kInvalidTexture)
        return offscreen_display_size_;
    return {static_cast<f32>(swapchain_width_) / dpi_scale_,
            static_cast<f32>(swapchain_height_) / dpi_scale_};
}

f32 RHI::Impl::dpi_scale() const {
    return dpi_scale_;
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

void RHI::EndOffscreen(RHITextureHandle target) {
    impl_->EndOffscreen(target);
}

void RHI::ConvertVideoFrame(RHITextureHandle target, RHITextureHandle y,
                              RHITextureHandle cb, RHITextureHandle cr) {
    impl_->ConvertVideoFrame(target, y, cb, cr);
}

Vec2 RHI::display_size() const { return impl_->display_size(); }
f32 RHI::dpi_scale() const { return impl_->dpi_scale(); }

} // namespace ugui
