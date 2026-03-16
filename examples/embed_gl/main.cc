// embed_gl: drop ultragui on top of an application that owns its own window,
// OpenGL context, and render pipeline (Dear ImGui-style embedding).
//
// The host here:
//   1. creates the GLFW window + GL 3.3 core context itself,
//   2. renders its own content (an animated fullscreen gradient) with its own
//      shader program every frame,
//   3. then calls ui.Update(), which composites the ultragui UI on top of the
//      host's framebuffer without clearing or presenting,
//   4. and presents the frame itself (glfwSwapBuffers).
//
// ultragui owns nothing: UIConfig.external_window attaches to the host window
// and UIConfig.embedded puts the RHI in "host owns the frame" mode.
//
// Build (OpenGL backend):
//   cmake -B build -G Ninja -DULTRAGUI_BACKEND_VULKAN=OFF \
//       -DULTRAGUI_BACKEND_OPENGL=ON \
//       -DULTRAGUI_RHI_SOURCE=$PWD/src/rhi/opengl/opengl_rhi.cc
//   cmake --build build --target ultragui_embed_gl

#define GLFW_INCLUDE_NONE
#include <ultragui/ultragui.h>

#include <GLFW/glfw3.h>
#include <cstdio>
#include <filesystem>

// --- Minimal GL 3.3 loader for the HOST's own pipeline ---------------------
// ultragui's RHI loads its own GL entry points internally; this is just the
// handful the host needs for its background shader.
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
#define GL_LINK_STATUS 0x8B82

#define GL_PROCS(X)                                                 \
  X(void, glViewport, (GLint, GLint, GLsizei, GLsizei))             \
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
  X(void, glUseProgram, (GLuint))                                   \
  X(void, glGenVertexArrays, (GLsizei, GLuint*))                    \
  X(void, glBindVertexArray, (GLuint))                              \
  X(GLint, glGetUniformLocation, (GLuint, const GLchar*))           \
  X(void, glUniform1f, (GLint, GLfloat))                            \
  X(void, glDrawArrays, (GLenum, GLint, GLsizei))

#define GL_DECL(ret, name, args) static ret(*name) args = nullptr;
GL_PROCS(GL_DECL)
#undef GL_DECL

static bool LoadGL() {
  bool ok = true;
#define GL_LOAD(ret, name, args)                                   \
  name = reinterpret_cast<ret(*) args>(glfwGetProcAddress(#name)); \
  ok = ok && (name != nullptr);
  GL_PROCS(GL_LOAD)
#undef GL_LOAD
  return ok;
}

static GLuint CompileShader(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    std::fprintf(stderr, "embed_gl: shader compile failed: %s\n", log);
  }
  return s;
}

static const char* kHostVert = R"(#version 330 core
out vec2 v_uv;
void main() {
  vec2 verts[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
  vec2 p = verts[gl_VertexID];
  v_uv = p * 0.5 + 0.5;
  gl_Position = vec4(p, 0.0, 1.0);
})";

static const char* kHostFrag = R"(#version 330 core
in vec2 v_uv;
uniform float u_time;
out vec4 frag;
void main() {
  vec3 c = 0.5 + 0.5 * cos(u_time + v_uv.xyx * 3.0 + vec3(0.0, 2.0, 4.0));
  frag = vec4(c * 0.22, 1.0);  // dim so the UI reads clearly on top
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

    text title { text: "ultragui, embedded"; font-size: 22; color: #ffffff; }
    text sub {
      text: "The window, GL context, and animated background are the host's.";
      font-size: 13; color: #a8a8c8;
    }
    button btn_hello {
      text: "Click me"; background: #4a4aff; color: #ffffff;
      corner-radius: 8; padding: 10 20; cursor: pointer;
    }
  }
}
)";

static const char* FindFont() {
  namespace fs = std::filesystem;
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

int main() {
  if (!glfwInit()) {
    std::fprintf(stderr, "embed_gl: glfwInit failed\n");
    return 1;
  }

  // The HOST sets up its own GL 3.3 core context.
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

  GLFWwindow* win = glfwCreateWindow(
      1280, 800, "ultragui embedded (host owns the GL pipeline)", nullptr,
      nullptr);
  if (!win) {
    std::fprintf(stderr, "embed_gl: window creation failed\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);

  if (!LoadGL()) {
    std::fprintf(stderr, "embed_gl: failed to load GL functions\n");
    return 1;
  }

  // The HOST's own render pipeline: a fullscreen animated gradient.
  GLuint prog = glCreateProgram();
  GLuint vs = CompileShader(GL_VERTEX_SHADER, kHostVert);
  GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kHostFrag);
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  GLint linked = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &linked);
  if (!linked) std::fprintf(stderr, "embed_gl: host program link failed\n");
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint u_time = glGetUniformLocation(prog, "u_time");
  GLuint vao = 0;
  glGenVertexArrays(1, &vao);

  // Attach ultragui to the host's window + GL context.
  ugui::UIContext ui;
  ugui::UIConfig cfg;
  cfg.external_window = win;  // do not create a window
  cfg.embedded = true;        // host clears + presents
  cfg.width = 1280;
  cfg.height = 800;
  if (!ui.Init(cfg)) {
    std::fprintf(stderr, "embed_gl: ui.Init failed\n");
    return 1;
  }

  if (const char* font = FindFont()) {
    ui.set_default_font(ui.LoadFont(font));
  } else {
    std::fprintf(stderr,
                 "embed_gl: no system font found; text will be blank\n");
  }
  ui.LoadUiString(kUi, "embed");

  int clicks = 0;
  ui.input().set_on_click([&](ugui::Widget* w, ugui::MouseButton) {
    if (w && w->name() == "btn_hello")
      std::printf("embed_gl: button clicked (%d)\n", ++clicks);
  });

  while (!glfwWindowShouldClose(win)) {
    int fbw, fbh;
    glfwGetFramebufferSize(win, &fbw, &fbh);

    // 1) the host renders its own content
    glViewport(0, 0, fbw, fbh);
    glUseProgram(prog);
    glUniform1f(u_time, static_cast<GLfloat>(glfwGetTime()));
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // 2) ultragui draws its UI on top (no clear, no present), and polls input
    //    from the shared window
    ui.Update();

    // 3) the host presents
    glfwSwapBuffers(win);
  }

  ui.Shutdown();
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
