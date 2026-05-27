// embed_opengl3: ultragui in Dear ImGui style on OpenGL 3.3 -- the host owns
// the window, GL context and its own render pipeline; ultragui produces a draw
// list (ui.RenderDrawData) that the ugui_impl_opengl3 backend renders into the
// host's framebuffer. (Unlike examples/embed_gl, ultragui creates no RHI here.)
//
// Build:  cmake --build build --target ultragui_embed_opengl3
// Run:    ./build/examples/ultragui_embed_opengl3

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <ugui/backends/ugui_impl_opengl3.h>
#include <ugui/ultragui.h>

// --- Minimal GL loader for the HOST's own pipeline (the backend loads its
// own).
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLchar = char;
#define GL_TRIANGLES 0x0004
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81

#define GL_PROCS(X)                                        \
  X(void, glViewport, (GLint, GLint, GLsizei, GLsizei))    \
  X(GLuint, glCreateShader, (GLenum))                      \
  X(void, glShaderSource,                                  \
    (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
  X(void, glCompileShader, (GLuint))                       \
  X(void, glGetShaderiv, (GLuint, GLenum, GLint*))         \
  X(GLuint, glCreateProgram, (void))                       \
  X(void, glAttachShader, (GLuint, GLuint))                \
  X(void, glLinkProgram, (GLuint))                         \
  X(void, glUseProgram, (GLuint))                          \
  X(void, glGenVertexArrays, (GLsizei, GLuint*))           \
  X(void, glBindVertexArray, (GLuint))                     \
  X(GLint, glGetUniformLocation, (GLuint, const GLchar*))  \
  X(void, glUniform1f, (GLint, GLfloat))                   \
  X(void, glDrawArrays, (GLenum, GLint, GLsizei))

#define GL_DECL(ret, name, args) static ret(*name) args = nullptr;
GL_PROCS(GL_DECL)
#undef GL_DECL

static void* LoadProc(const char* n) {
  return reinterpret_cast<void*>(glfwGetProcAddress(n));
}
static bool LoadHostGL() {
  bool ok = true;
#define GL_LOAD(ret, name, args)                         \
  name = reinterpret_cast<ret(*) args>(LoadProc(#name)); \
  ok = ok && (name != nullptr);
  GL_PROCS(GL_LOAD)
#undef GL_LOAD
  return ok;
}

static const char* kHostVert = R"(#version 330 core
out vec2 v_uv;
void main() {
  vec2 v[3] = vec2[](vec2(-1.0,-1.0), vec2(3.0,-1.0), vec2(-1.0,3.0));
  v_uv = v[gl_VertexID] * 0.5 + 0.5;
  gl_Position = vec4(v[gl_VertexID], 0.0, 1.0);
})";
static const char* kHostFrag = R"(#version 330 core
in vec2 v_uv; uniform float u_time; out vec4 frag;
void main() {
  vec3 c = 0.5 + 0.5 * cos(u_time + v_uv.xyx * 3.0 + vec3(0.0, 2.0, 4.0));
  frag = vec4(c * 0.22, 1.0);
})";

static const char* kUi = R"(
panel root {
  width: 100vw; height: 100vh;
  layout: column; justify: center; align: center;
  panel card {
    layout: column; gap: 10; padding: 28;
    background: #14142899; corner-radius: 18;
    border-color: #ffffff20; border-width: 1;
    shadow-color: #00000080; shadow-blur: 28; shadow-y: 10;
    text title { text: "ultragui via ugui_impl_opengl3"; font-size: 22; color: #ffffff; }
    text sub { text: "Host owns the GL context; ultragui hands it a draw list.";
               font-size: 13; color: #a8a8c8; }
    button btn_hello { text: "Click me"; background: #4a4aff; color: #ffffff;
                       corner-radius: 8; padding: 10 20; cursor: pointer; }
  }
}
)";

static const char* FindFont() {
  namespace fs = std::filesystem;
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
  return nullptr;
}

int main() {
  if (!glfwInit()) {
    std::fprintf(stderr, "embed_opengl3: glfwInit failed\n");
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
  GLFWwindow* win = glfwCreateWindow(
      1280, 800, "ultragui embedded (OpenGL3 draw-data)", nullptr, nullptr);
  if (!win) {
    std::fprintf(stderr, "embed_opengl3: window creation failed\n");
    return 1;
  }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);
  if (!LoadHostGL()) {
    std::fprintf(stderr, "embed_opengl3: failed to load host GL\n");
    return 1;
  }

  // Host's own pipeline (animated fullscreen gradient).
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs, 1, &kHostVert, nullptr);
  glCompileShader(vs);
  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fs, 1, &kHostFrag, nullptr);
  glCompileShader(fs);
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  GLint u_time = glGetUniformLocation(prog, "u_time");
  GLuint vao = 0;
  glGenVertexArrays(1, &vao);

  // ultragui in draw-data mode (no graphics device of its own).
  ugui::UIContext ui;
  ugui::UIConfig cfg;
  cfg.external_window = win;
  cfg.draw_data = true;
  cfg.width = 1280;
  cfg.height = 800;
  if (!ui.Init(cfg)) {
    std::fprintf(stderr, "embed_opengl3: ui.Init failed\n");
    return 1;
  }
  if (const char* font = FindFont())
    ui.set_default_font(ui.LoadFont(font));
  else
    std::fprintf(stderr, "embed_opengl3: no system font found\n");
  ui.LoadUiString(kUi, "embed");

  int clicks = 0;
  ui.input().set_on_click([&](ugui::wid w, ugui::MouseButton) {
    ugui::WidgetNode* n = ui.world().Get<ugui::WidgetNode>(w);
    if (n && n->name == "btn_hello")
      std::printf("embed_opengl3: button clicked (%d)\n", ++clicks);
  });

  // ultragui OpenGL3 backend.
  ugui::gl::InitInfo bi;
  bi.get_proc_address = LoadProc;
  if (!ugui::gl::Init(bi)) {
    std::fprintf(stderr, "embed_opengl3: backend init failed\n");
    return 1;
  }
  ugui::u32 font_rev = ~0u;

  while (!glfwWindowShouldClose(win)) {
    const ugui::DrawData& dd = ui.RenderDrawData();  // polls input, builds UI
    if (ui.text_engine().atlas_revision() != font_rev) {
      ugui::Vec2 as = ui.text_engine().atlas_size();
      ugui::gl::UpdateFontAtlas(ui.text_engine().atlas_pixels(),
                                static_cast<ugui::u32>(as.x),
                                static_cast<ugui::u32>(as.y));
      font_rev = ui.text_engine().atlas_revision();
    }

    int fbw, fbh;
    glfwGetFramebufferSize(win, &fbw, &fbh);

    // 1) host renders its own content
    glViewport(0, 0, fbw, fbh);
    glUseProgram(prog);
    glUniform1f(u_time, static_cast<float>(glfwGetTime()));
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // 2) ultragui on top via the draw-data backend
    ugui::gl::NewFrame();
    ugui::gl::RenderDrawData(dd);

    glfwSwapBuffers(win);
  }

  ugui::gl::Shutdown();
  ui.Shutdown();
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
