// ugui_impl_opengl3: OpenGL 3.3 core renderer backend for ultragui (see
// header). Uses the same GL-330 shaders and Vertex2D layout as the bundled
// OpenGL RHI so it renders ultragui::DrawData identically, into the host's
// framebuffer.

#include <cstddef>
#include <cstdio>
#include <ugui/backends/ugui_impl_opengl3.h>
#include <ugui/render/vertex.h>

namespace ugui {
namespace gl {
namespace {

// --- minimal GL 3.3 typedefs / constants (no GL header dependency) ---
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLchar = char;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLintptr = ptrdiff_t;
using GLsizeiptr = ptrdiff_t;

#define GL_FALSE 0
#define GL_TRIANGLES 0x0004
#define GL_TEXTURE_2D 0x0DE1
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_TEXTURE0 0x84C0
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_INT 0x1405
#define GL_UNSIGNED_BYTE 0x1401
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_RED 0x1903
#define GL_R8 0x8229
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_SRC_ALPHA 0x0302
#define GL_ONE 1
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_FUNC_ADD 0x8006

#define GL_PROCS(X)                                                 \
  X(void, glGenVertexArrays, (GLsizei, GLuint*))                    \
  X(void, glBindVertexArray, (GLuint))                              \
  X(void, glDeleteVertexArrays, (GLsizei, const GLuint*))           \
  X(void, glGenBuffers, (GLsizei, GLuint*))                         \
  X(void, glBindBuffer, (GLenum, GLuint))                           \
  X(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum))  \
  X(void, glDeleteBuffers, (GLsizei, const GLuint*))                \
  X(void, glGenTextures, (GLsizei, GLuint*))                        \
  X(void, glBindTexture, (GLenum, GLuint))                          \
  X(void, glDeleteTextures, (GLsizei, const GLuint*))               \
  X(void, glTexImage2D,                                             \
    (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, \
     const void*))                                                  \
  X(void, glTexParameteri, (GLenum, GLenum, GLint))                 \
  X(void, glActiveTexture, (GLenum))                                \
  X(void, glPixelStorei, (GLenum, GLint))                           \
  X(GLuint, glCreateShader, (GLenum))                               \
  X(void, glShaderSource,                                           \
    (GLuint, GLsizei, const GLchar* const*, const GLint*))          \
  X(void, glCompileShader, (GLuint))                                \
  X(void, glGetShaderiv, (GLuint, GLenum, GLint*))                  \
  X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
  X(GLuint, glCreateProgram, (void))                                \
  X(void, glAttachShader, (GLuint, GLuint))                         \
  X(void, glLinkProgram, (GLuint))                                  \
  X(void, glGetProgramiv, (GLuint, GLenum, GLint*))                 \
  X(void, glDeleteShader, (GLuint))                                 \
  X(void, glDeleteProgram, (GLuint))                                \
  X(void, glUseProgram, (GLuint))                                   \
  X(GLint, glGetUniformLocation, (GLuint, const GLchar*))           \
  X(void, glUniform1i, (GLint, GLint))                              \
  X(void, glUniform2f, (GLint, GLfloat, GLfloat))                   \
  X(void, glEnableVertexAttribArray, (GLuint))                      \
  X(void, glVertexAttribPointer,                                    \
    (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))       \
  X(void, glVertexAttribIPointer,                                   \
    (GLuint, GLint, GLenum, GLsizei, const void*))                  \
  X(void, glEnable, (GLenum))                                       \
  X(void, glDisable, (GLenum))                                      \
  X(void, glBlendEquation, (GLenum))                                \
  X(void, glBlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum))    \
  X(void, glViewport, (GLint, GLint, GLsizei, GLsizei))             \
  X(void, glScissor, (GLint, GLint, GLsizei, GLsizei))              \
  X(void, glDrawElements, (GLenum, GLsizei, GLenum, const void*))

#define GL_DECL(ret, name, args) ret(*name) args = nullptr;
GL_PROCS(GL_DECL)
#undef GL_DECL

struct State {
  GLuint vao = 0;
  GLuint quad_vbo = 0, quad_ibo = 0, text_vbo = 0, text_ibo = 0;
  GLuint quad_prog = 0, text_prog = 0;
  GLint quad_scale = -1, quad_translate = -1, quad_tex = -1;
  GLint text_scale = -1, text_translate = -1, text_tex = -1;
  GLuint white = 0, font = 0;
};
State g;

bool LoadGL(void* (*loader)(const char*)) {
  bool ok = true;
#define GL_LOAD(ret, name, args)                       \
  name = reinterpret_cast<ret(*) args>(loader(#name)); \
  ok = ok && (name != nullptr);
  GL_PROCS(GL_LOAD)
#undef GL_LOAD
  return ok;
}

const char* kQuadVert = R"glsl(
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
    vec4 col = vec4(float(c & 0xFFu)/255.0, float((c>>8)&0xFFu)/255.0,
                    float((c>>16)&0xFFu)/255.0, float((c>>24)&0xFFu)/255.0);
    col.rgb = srgb_to_linear(col.rgb);
    return col;
}
void main() {
    gl_Position = vec4(in_pos * u_scale + u_translate, 0.0, 1.0);
    frag_uv = in_uv;
    frag_color = unpack_color(in_color);
    frag_color2 = unpack_color(in_color2);
    frag_corner_radii = vec4(float(in_corner_radii & 0xFFu),
                             float((in_corner_radii>>8)&0xFFu),
                             float((in_corner_radii>>16)&0xFFu),
                             float((in_corner_radii>>24)&0xFFu));
    frag_softness = in_softness;
    frag_half_size = in_half_size;
    frag_border_width = in_border_width;
    frag_border_color = unpack_color(in_border_color);
}
)glsl";

const char* kQuadFrag = R"glsl(
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
        float alpha = (frag_softness < 0.0) ? smoothstep(-aa, 0.0, d)
                                            : 1.0 - smoothstep(-aa, aa, d);
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

const char* kTextVert = R"glsl(
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
    vec4 col = vec4(float(in_color & 0xFFu)/255.0, float((in_color>>8)&0xFFu)/255.0,
                    float((in_color>>16)&0xFFu)/255.0, float((in_color>>24)&0xFFu)/255.0);
    col.rgb = srgb_to_linear(col.rgb);
    frag_color = col;
}
)glsl";

const char* kTextFrag = R"glsl(
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

GLuint Compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    std::fprintf(stderr, "ugui_impl_opengl3: shader compile failed: %s\n", log);
  }
  return s;
}

GLuint Program(const char* vs_src, const char* fs_src) {
  GLuint vs = Compile(GL_VERTEX_SHADER, vs_src);
  GLuint fs = Compile(GL_FRAGMENT_SHADER, fs_src);
  GLuint p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) std::fprintf(stderr, "ugui_impl_opengl3: program link failed\n");
  glDeleteShader(vs);
  glDeleteShader(fs);
  return p;
}

GLuint MakeTexture(u32 w, u32 h, GLint internal, GLenum fmt, const void* px,
                   GLint filter) {
  GLuint t = 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, internal, static_cast<GLsizei>(w),
               static_cast<GLsizei>(h), 0, fmt, GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return t;
}

// Bind a VBO/IBO and (re)point the 9 Vertex2D attributes at it.
void BindFormat(GLuint vbo, GLuint ibo) {
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  auto off = [](size_t o) { return reinterpret_cast<const void*>(o); };
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D),
                        off(offsetof(Vertex2D, pos)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D),
                        off(offsetof(Vertex2D, uv)));
  glEnableVertexAttribArray(2);
  glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(Vertex2D),
                         off(offsetof(Vertex2D, color)));
  glEnableVertexAttribArray(3);
  glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(Vertex2D),
                         off(offsetof(Vertex2D, color2)));
  glEnableVertexAttribArray(4);
  glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(Vertex2D),
                         off(offsetof(Vertex2D, corner_radii)));
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex2D),
                        off(offsetof(Vertex2D, softness)));
  glEnableVertexAttribArray(6);
  glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D),
                        off(offsetof(Vertex2D, half_size)));
  glEnableVertexAttribArray(7);
  glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex2D),
                        off(offsetof(Vertex2D, border_width)));
  glEnableVertexAttribArray(8);
  glVertexAttribIPointer(8, 1, GL_UNSIGNED_INT, sizeof(Vertex2D),
                         off(offsetof(Vertex2D, border_color)));
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
}

void Upload(GLuint buf, GLenum target, const void* data, GLsizeiptr bytes) {
  glBindBuffer(target, buf);
  glBufferData(target, bytes, data, GL_DYNAMIC_DRAW);
}

}  // namespace

bool Init(const InitInfo& info) {
  if (!info.get_proc_address) return false;
  if (!LoadGL(info.get_proc_address)) {
    std::fprintf(stderr, "ugui_impl_opengl3: failed to load GL functions\n");
    return false;
  }
  g.quad_prog = Program(kQuadVert, kQuadFrag);
  g.text_prog = Program(kTextVert, kTextFrag);
  g.quad_scale = glGetUniformLocation(g.quad_prog, "u_scale");
  g.quad_translate = glGetUniformLocation(g.quad_prog, "u_translate");
  g.quad_tex = glGetUniformLocation(g.quad_prog, "tex_sampler");
  g.text_scale = glGetUniformLocation(g.text_prog, "u_scale");
  g.text_translate = glGetUniformLocation(g.text_prog, "u_translate");
  g.text_tex = glGetUniformLocation(g.text_prog, "tex_sampler");
  glUseProgram(g.quad_prog);
  glUniform1i(g.quad_tex, 0);
  glUseProgram(g.text_prog);
  glUniform1i(g.text_tex, 0);

  glGenVertexArrays(1, &g.vao);
  glGenBuffers(1, &g.quad_vbo);
  glGenBuffers(1, &g.quad_ibo);
  glGenBuffers(1, &g.text_vbo);
  glGenBuffers(1, &g.text_ibo);

  u32 white = 0xFFFFFFFFu;
  g.white = MakeTexture(1, 1, GL_RGBA8, GL_RGBA, &white, GL_LINEAR);
  return true;
}

void Shutdown() {
  if (g.vao) glDeleteVertexArrays(1, &g.vao);
  GLuint bufs[] = {g.quad_vbo, g.quad_ibo, g.text_vbo, g.text_ibo};
  glDeleteBuffers(4, bufs);
  if (g.white) glDeleteTextures(1, &g.white);
  if (g.font) glDeleteTextures(1, &g.font);
  if (g.quad_prog) glDeleteProgram(g.quad_prog);
  if (g.text_prog) glDeleteProgram(g.text_prog);
  g = State{};
}

void NewFrame() {}

bool UpdateFontAtlas(const u8* pixels, u32 width, u32 height) {
  if (!pixels || width == 0 || height == 0) return false;
  if (g.font) glDeleteTextures(1, &g.font);
  g.font = MakeTexture(width, height, GL_R8, GL_RED, pixels, GL_NEAREST);
  return g.font != 0;
}

void RenderDrawData(const DrawData& dd) {
  if (!dd.valid || dd.command_count == 0) return;
  if (dd.display_size.x <= 0.0f || dd.display_size.y <= 0.0f) return;

  f32 sx = dd.framebuffer_scale.x > 0 ? dd.framebuffer_scale.x : 1.0f;
  f32 sy = dd.framebuffer_scale.y > 0 ? dd.framebuffer_scale.y : 1.0f;
  GLsizei fbw = static_cast<GLsizei>(dd.display_size.x * sx);
  GLsizei fbh = static_cast<GLsizei>(dd.display_size.y * sy);

  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_SCISSOR_TEST);
  glViewport(0, 0, fbw, fbh);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(g.vao);

  Upload(g.quad_vbo, GL_ARRAY_BUFFER, dd.quad_vertices,
         static_cast<GLsizeiptr>(dd.quad_vertex_count) * sizeof(Vertex2D));
  Upload(g.quad_ibo, GL_ELEMENT_ARRAY_BUFFER, dd.quad_indices,
         static_cast<GLsizeiptr>(dd.quad_index_count) * sizeof(u32));
  Upload(g.text_vbo, GL_ARRAY_BUFFER, dd.text_vertices,
         static_cast<GLsizeiptr>(dd.text_vertex_count) * sizeof(Vertex2D));
  Upload(g.text_ibo, GL_ELEMENT_ARRAY_BUFFER, dd.text_indices,
         static_cast<GLsizeiptr>(dd.text_index_count) * sizeof(u32));

  // Projection: window coords -> NDC, flipping Y for GL's bottom-left origin.
  float scale_x = 2.0f / dd.display_size.x;
  float scale_y = -2.0f / dd.display_size.y;
  glUseProgram(g.quad_prog);
  glUniform2f(g.quad_scale, scale_x, scale_y);
  glUniform2f(g.quad_translate, -1.0f, 1.0f);
  glUseProgram(g.text_prog);
  glUniform2f(g.text_scale, scale_x, scale_y);
  glUniform2f(g.text_translate, -1.0f, 1.0f);

  int bound_kind = -1;  // 0 quad, 1 text
  GLuint bound_prog = 0;
  for (u32 i = 0; i < dd.command_count; ++i) {
    const DrawCmd& c = dd.commands[i];
    if (c.elem_count == 0) continue;

    GLuint prog = c.is_text ? g.text_prog : g.quad_prog;
    if (prog != bound_prog) {
      glUseProgram(prog);
      bound_prog = prog;
    }
    GLuint tex =
        (c.is_text || c.texture_id == kFontTextureId) ? g.font : g.white;
    glBindTexture(GL_TEXTURE_2D, tex);

    int kind = c.is_text ? 1 : 0;
    if (kind != bound_kind) {
      if (kind)
        BindFormat(g.text_vbo, g.text_ibo);
      else
        BindFormat(g.quad_vbo, g.quad_ibo);
      bound_kind = kind;
    }

    // GL scissor: bottom-left origin, so flip Y.
    GLint x = static_cast<GLint>(c.clip_rect.x * sx);
    GLint w = static_cast<GLint>(c.clip_rect.w * sx);
    GLint h = static_cast<GLint>(c.clip_rect.h * sy);
    GLint y = fbh - static_cast<GLint>((c.clip_rect.y + c.clip_rect.h) * sy);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    glScissor(x, y, w, h);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(c.elem_count),
                   GL_UNSIGNED_INT,
                   reinterpret_cast<const void*>(
                       static_cast<size_t>(c.index_offset) * sizeof(u32)));
  }
}

}  // namespace gl
}  // namespace ugui
