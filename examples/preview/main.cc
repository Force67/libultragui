// ugui_preview: render a .ugui file standalone, either as a live window with
// hot reload or headless to a PNG. Built for design iteration loops (tweak
// markup, screenshot, compare) without writing a host application.
//
// Usage:
//   ugui_preview ui.ugui                          # live window, hot reload
//   ugui_preview ui.ugui --screenshot out.png     # render offscreen, save, exit
//   ugui_preview ui.ugui --size 1280x800 --bg #101020 --frames 3
//   ugui_preview ui.ugui --script logic.lua --font /path/to/font.ttf
//
// A sibling <name>.lua next to the .ugui file is loaded automatically.
// Rendering goes through the OpenGL3 draw-data backend (no Vulkan needed).

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <ugui/backends/ugui_impl_opengl3.h>
#include <ugui/ultragui.h>

// --- GL loader for the FBO/readback path (the backend loads its own) --------

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLbitfield = unsigned int;
using GLfloat = float;

#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_NEAREST 0x2600
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_PACK_ALIGNMENT 0x0D05

#define GL_PROCS(X)                                                     \
  X(void, glGenTextures, (GLsizei, GLuint*))                            \
  X(void, glBindTexture, (GLenum, GLuint))                              \
  X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, \
                         GLenum, GLenum, const void*))                  \
  X(void, glTexParameteri, (GLenum, GLenum, GLint))                     \
  X(void, glGenFramebuffers, (GLsizei, GLuint*))                        \
  X(void, glBindFramebuffer, (GLenum, GLuint))                          \
  X(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
  X(GLenum, glCheckFramebufferStatus, (GLenum))                         \
  X(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat))           \
  X(void, glClear, (GLbitfield))                                        \
  X(void, glPixelStorei, (GLenum, GLint))                               \
  X(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, \
                         void*))                                        \
  X(void, glFinish, (void))

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

// --- Dependency-free PNG writer (uncompressed zlib stored blocks) -----------

static ugui::u32 Crc32(const unsigned char* data, size_t len, ugui::u32 crc) {
  static ugui::u32 table[256];
  static bool init = false;
  if (!init) {
    for (ugui::u32 i = 0; i < 256; ++i) {
      ugui::u32 c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      table[i] = c;
    }
    init = true;
  }
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

static void PutU32BE(std::vector<unsigned char>& v, ugui::u32 x) {
  v.push_back(static_cast<unsigned char>(x >> 24));
  v.push_back(static_cast<unsigned char>(x >> 16));
  v.push_back(static_cast<unsigned char>(x >> 8));
  v.push_back(static_cast<unsigned char>(x));
}

static void PutChunk(std::vector<unsigned char>& out, const char type[4],
                     const std::vector<unsigned char>& data) {
  PutU32BE(out, static_cast<ugui::u32>(data.size()));
  size_t crc_start = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  PutU32BE(out, Crc32(out.data() + crc_start, out.size() - crc_start, 0));
}

// rows: top-down RGBA8, width*4 bytes each
static bool WritePng(const char* path, const unsigned char* rgba, int width,
                     int height) {
  // Raw scanline stream: filter byte 0 + row, per row
  std::vector<unsigned char> raw;
  raw.reserve(static_cast<size_t>(height) * (1 + width * 4));
  for (int y = 0; y < height; ++y) {
    raw.push_back(0);
    const unsigned char* row = rgba + static_cast<size_t>(y) * width * 4;
    raw.insert(raw.end(), row, row + width * 4);
  }

  ugui::u32 a = 1, b = 0;  // adler32
  for (unsigned char c : raw) {
    a = (a + c) % 65521;
    b = (b + a) % 65521;
  }

  std::vector<unsigned char> idat;
  idat.push_back(0x78);
  idat.push_back(0x01);
  size_t pos = 0;
  while (pos < raw.size()) {
    size_t n = raw.size() - pos;
    if (n > 65535) n = 65535;
    bool final = pos + n == raw.size();
    idat.push_back(final ? 1 : 0);
    idat.push_back(static_cast<unsigned char>(n & 0xFF));
    idat.push_back(static_cast<unsigned char>(n >> 8));
    idat.push_back(static_cast<unsigned char>(~n & 0xFF));
    idat.push_back(static_cast<unsigned char>((~n >> 8) & 0xFF));
    idat.insert(idat.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
    pos += n;
  }
  PutU32BE(idat, (b << 16) | a);

  std::vector<unsigned char> ihdr;
  PutU32BE(ihdr, static_cast<ugui::u32>(width));
  PutU32BE(ihdr, static_cast<ugui::u32>(height));
  ihdr.push_back(8);  // bit depth
  ihdr.push_back(6);  // color type RGBA
  ihdr.push_back(0);
  ihdr.push_back(0);
  ihdr.push_back(0);

  std::vector<unsigned char> png = {137, 80, 78, 71, 13, 10, 26, 10};
  PutChunk(png, "IHDR", ihdr);
  PutChunk(png, "IDAT", idat);
  PutChunk(png, "IEND", {});

  FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  size_t written = std::fwrite(png.data(), 1, png.size(), f);
  std::fclose(f);
  return written == png.size();
}

// --- Helpers -----------------------------------------------------------------

static const char* FindFont() {
  static std::string resolved;
  if (FILE* p = popen("fc-match -f '%{file}' sans 2>/dev/null", "r")) {
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, p);
    pclose(p);
    if (n > 0) {
      buf[n] = '\0';
      resolved = buf;
      if (std::filesystem::exists(resolved)) return resolved.c_str();
    }
  }
  return nullptr;
}

static ugui::Color ParseHexColor(const char* s) {
  if (*s == '#') ++s;
  ugui::u32 hex = static_cast<ugui::u32>(std::strtoul(s, nullptr, 16));
  return ugui::Color::FromHex(hex);
}

int main(int argc, char** argv) {
  const char* ugui_path = nullptr;
  const char* script_path = nullptr;
  const char* screenshot_path = nullptr;
  const char* font_path = nullptr;
  int width = 1280, height = 800;
  int frames = 3;
  ugui::Color bg = ugui::Color::FromHex(0x0f0f1a);

  for (int i = 1; i < argc; ++i) {
    auto next = [&](const char* flag) -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "ugui_preview: %s needs a value\n", flag);
        std::exit(2);
      }
      return argv[++i];
    };
    if (!std::strcmp(argv[i], "--screenshot")) {
      screenshot_path = next("--screenshot");
    } else if (!std::strcmp(argv[i], "--script")) {
      script_path = next("--script");
    } else if (!std::strcmp(argv[i], "--font")) {
      font_path = next("--font");
    } else if (!std::strcmp(argv[i], "--size")) {
      if (std::sscanf(next("--size"), "%dx%d", &width, &height) != 2) {
        std::fprintf(stderr, "ugui_preview: --size expects WxH\n");
        return 2;
      }
    } else if (!std::strcmp(argv[i], "--frames")) {
      frames = std::atoi(next("--frames"));
    } else if (!std::strcmp(argv[i], "--bg")) {
      bg = ParseHexColor(next("--bg"));
    } else if (argv[i][0] != '-') {
      ugui_path = argv[i];
    } else {
      std::fprintf(stderr, "ugui_preview: unknown flag %s\n", argv[i]);
      return 2;
    }
  }
  if (!ugui_path) {
    std::fprintf(stderr,
                 "usage: ugui_preview <file.ugui> [--screenshot out.png] "
                 "[--size WxH] [--frames N] [--bg #rrggbb] [--script f.lua] "
                 "[--font f.ttf]\n");
    return 2;
  }

  if (!glfwInit()) {
    std::fprintf(stderr, "ugui_preview: glfwInit failed\n");
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  // Live mode stays resizable so %, vw/vh, and @media breakpoints can be
  // exercised by dragging the window; screenshots need the exact size.
  if (screenshot_path) {
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  }
  GLFWwindow* win =
      glfwCreateWindow(width, height, "ugui_preview", nullptr, nullptr);
  if (!win) {
    std::fprintf(stderr, "ugui_preview: window creation failed\n");
    return 1;
  }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);
  if (!LoadHostGL()) {
    std::fprintf(stderr, "ugui_preview: failed to load GL\n");
    return 1;
  }

  ugui::UIContext ui;
  ugui::UIConfig cfg;
  cfg.external_window = win;
  cfg.draw_data = true;
  cfg.width = width;
  cfg.height = height;
  cfg.clear_color = bg;
  if (!ui.Init(cfg)) {
    std::fprintf(stderr, "ugui_preview: ui.Init failed\n");
    return 1;
  }

  if (!font_path) font_path = FindFont();
  if (font_path)
    ui.set_default_font(ui.LoadFont(font_path));
  else
    std::fprintf(stderr, "ugui_preview: no font found (use --font)\n");

  if (!ui.LoadUi(ugui_path).valid()) {
    std::fprintf(stderr, "ugui_preview: failed to load %s\n", ugui_path);
    return 1;
  }

  // Auto-load a sibling .lua with the same stem, or the explicit --script.
  std::filesystem::path lua_file =
      script_path ? std::filesystem::path(script_path)
                  : std::filesystem::path(ugui_path).replace_extension(".lua");
  if (std::filesystem::exists(lua_file)) ui.LoadScript(lua_file.c_str());

  ui.input().set_on_click([&](ugui::wid w, ugui::MouseButton) {
    ugui::WidgetNode* n = ui.world().Get<ugui::WidgetNode>(w);
    if (n && !n->name.empty())
      ui.script().CallHandler(("on_" + n->name).c_str(), w);
  });

  ugui::gl::InitInfo bi;
  bi.get_proc_address = LoadProc;
  if (!ugui::gl::Init(bi)) {
    std::fprintf(stderr, "ugui_preview: GL backend init failed\n");
    return 1;
  }
  ui.set_texture_backend(&ugui::gl::texture_backend());

  int fbw, fbh;
  glfwGetFramebufferSize(win, &fbw, &fbh);

  // Offscreen target for screenshot mode: rendering into our own FBO avoids
  // pixel-ownership issues of reading back a hidden window's backbuffer.
  GLuint fbo = 0, fbo_tex = 0;
  if (screenshot_path) {
    glGenTextures(1, &fbo_tex);
    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fbw, fbh, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           fbo_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      std::fprintf(stderr, "ugui_preview: FBO incomplete\n");
      return 1;
    }
  }

  ugui::u32 font_rev = ~0u;
  auto frame = [&] {
    const ugui::DrawData& dd = ui.RenderDrawData();
    if (ui.text_engine().atlas_revision() != font_rev) {
      ugui::Vec2 as = ui.text_engine().atlas_size();
      ugui::gl::UpdateFontAtlas(ui.text_engine().atlas_pixels(),
                                static_cast<ugui::u32>(as.x),
                                static_cast<ugui::u32>(as.y));
      font_rev = ui.text_engine().atlas_revision();
    }
    glClearColor(bg.r, bg.g, bg.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ugui::gl::NewFrame();
    ugui::gl::RenderDrawData(dd);
  };

  int exit_code = 0;
  if (screenshot_path) {
    if (frames < 1) frames = 1;
    for (int i = 0; i < frames; ++i) frame();
    glFinish();

    std::vector<unsigned char> pixels(static_cast<size_t>(fbw) * fbh * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // GL rows are bottom-up; PNG wants top-down
    std::vector<unsigned char> flipped(pixels.size());
    size_t stride = static_cast<size_t>(fbw) * 4;
    for (int y = 0; y < fbh; ++y)
      std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                  pixels.data() + static_cast<size_t>(fbh - 1 - y) * stride,
                  stride);

    if (WritePng(screenshot_path, flipped.data(), fbw, fbh)) {
      std::printf("ugui_preview: wrote %s (%dx%d)\n", screenshot_path, fbw,
                  fbh);
    } else {
      std::fprintf(stderr, "ugui_preview: failed to write %s\n",
                   screenshot_path);
      exit_code = 1;
    }
  } else {
    // Live preview with hot reload on file change
    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(ugui_path, ec);
    int poll = 0;
    while (!glfwWindowShouldClose(win)) {
      if (++poll >= 30) {
        poll = 0;
        auto now = std::filesystem::last_write_time(ugui_path, ec);
        if (!ec && now != mtime) {
          mtime = now;
          ui.LoadUi(ugui_path);
          if (std::filesystem::exists(lua_file)) ui.LoadScript(lua_file.c_str());
          std::printf("ugui_preview: reloaded %s\n", ugui_path);
        }
      }
      frame();
      glfwSwapBuffers(win);
    }
  }

  ugui::gl::Shutdown();
  ui.Shutdown();
  glfwDestroyWindow(win);
  glfwTerminate();
  return exit_code;
}
