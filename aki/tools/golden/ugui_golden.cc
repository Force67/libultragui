// Golden runner, C++ side: plays a scenario against the C++ libultragui in
// draw-data mode on the host platform and writes everything the frame hands a
// renderer to one binary file. aki/tools/golden/aki_golden plays the same
// scenario against the aki port; compare.py diffs the two files.
//
//   ugui_golden <scenario.txt> <out.bin>
//
// The scenario format is in README.md. The output format, little endian:
//
//   "UGGD" u32 version(1)
//   records, each starting with one tag byte:
//     'T' u64 id, u32 w, u32 h, u8 format, u8 filter, u64 bytes, u64 fnv1a  texture created
//     'U' u64 id, u64 bytes, u64 fnv1a                                     texture updated
//     'X' u64 id                                                          texture destroyed
//     'A' u32 revision, u32 w, u32 h, u64 fnv1a                            glyph atlas changed
//     'F' u32 frame, f32 display_pos[2], f32 display_size[2], f32 fb_scale[2], u8 valid,
//         u32 nqv, nqv * 48 bytes, u32 nqi, nqi * u32,
//         u32 ntv, ntv * 48 bytes, u32 nti, nti * u32,
//         u32 ncmd, ncmd * (f32 clip[4], u64 tex, u32 off, u32 count, u8 is_text, f32 blur)
//     'E'                                                                 end
#include <ugui/platform/platform_host.h>
#include <ugui/ui_context.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

extern double ugui_golden_time;

namespace {

using namespace ugui;

struct Writer {
  FILE* f = nullptr;
  void bytes(const void* p, size_t n) { fwrite(p, 1, n, f); }
  void u8v(uint8_t v) { bytes(&v, 1); }
  void u32v(uint32_t v) { bytes(&v, 4); }
  void u64v(uint64_t v) { bytes(&v, 8); }
  void f32v(float v) { bytes(&v, 4); }
};

uint64_t Fnv1a(const void* data, size_t n) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 1099511628211ull;
  }
  return h;
}

size_t BytesPerPixel(RHIFormat format) {
  switch (format) {
    case RHIFormat::kR8Unorm:
      return 1;
    case RHIFormat::kRgba8Unorm:
    case RHIFormat::kBgra8Unorm:
      return 4;
    case RHIFormat::kRgba32Float:
      return 16;
  }
  return 4;
}

// Mints ids 1, 2, 3, ... and records every upload, so the dump shows what the
// UI rasterized (SVGs, gradients, images) and not only that it drew a quad.
struct RecordingTextures : TextureBackend {
  Writer* out = nullptr;
  struct Info {
    uint32_t w, h;
    RHIFormat format;
  };
  std::vector<Info> infos;  // index id - 1

  TextureId CreateTexture(u32 width, u32 height, RHIFormat format,
                          const void* pixels, RHIFilter filter) override {
    infos.push_back({width, height, format});
    const TextureId id = infos.size();
    const uint64_t n = uint64_t(width) * height * BytesPerPixel(format);
    out->u8v('T');
    out->u64v(id);
    out->u32v(width);
    out->u32v(height);
    out->u8v(static_cast<uint8_t>(format));
    out->u8v(static_cast<uint8_t>(filter));
    out->u64v(pixels ? n : 0);
    out->u64v(pixels ? Fnv1a(pixels, n) : 0);
    return id;
  }
  void UpdateTexture(TextureId id, const void* pixels) override {
    uint64_t n = 0, h = 0;
    if (id >= 1 && id <= infos.size() && pixels) {
      const Info& i = infos[id - 1];
      n = uint64_t(i.w) * i.h * BytesPerPixel(i.format);
      h = Fnv1a(pixels, n);
    }
    out->u8v('U');
    out->u64v(id);
    out->u64v(n);
    out->u64v(h);
  }
  void DestroyTexture(TextureId id) override {
    out->u8v('X');
    out->u64v(id);
  }
};

void DumpFrame(Writer& w, uint32_t frame, const DrawData& dd) {
  w.u8v('F');
  w.u32v(frame);
  w.f32v(dd.display_pos.x);
  w.f32v(dd.display_pos.y);
  w.f32v(dd.display_size.x);
  w.f32v(dd.display_size.y);
  w.f32v(dd.framebuffer_scale.x);
  w.f32v(dd.framebuffer_scale.y);
  w.u8v(dd.valid ? 1 : 0);
  static_assert(sizeof(Vertex2D) == 48, "dump assumes the 48-byte vertex");
  w.u32v(dd.quad_vertex_count);
  w.bytes(dd.quad_vertices, size_t(dd.quad_vertex_count) * 48);
  w.u32v(dd.quad_index_count);
  w.bytes(dd.quad_indices, size_t(dd.quad_index_count) * 4);
  w.u32v(dd.text_vertex_count);
  w.bytes(dd.text_vertices, size_t(dd.text_vertex_count) * 48);
  w.u32v(dd.text_index_count);
  w.bytes(dd.text_indices, size_t(dd.text_index_count) * 4);
  w.u32v(dd.command_count);
  for (u32 i = 0; i < dd.command_count; ++i) {
    const DrawCmd& c = dd.commands[i];
    w.f32v(c.clip_rect.x);
    w.f32v(c.clip_rect.y);
    w.f32v(c.clip_rect.w);
    w.f32v(c.clip_rect.h);
    w.u64v(c.texture_id);
    w.u32v(c.index_offset);
    w.u32v(c.elem_count);
    w.u8v(c.is_text ? 1 : 0);
    w.f32v(c.blur);
  }
}

MouseButton ParseButton(const std::string& s) {
  if (s == "right") return MouseButton::kRight;
  if (s == "middle") return MouseButton::kMiddle;
  return MouseButton::kLeft;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: ugui_golden <scenario.txt> <out.bin>\n");
    return 2;
  }
  std::ifstream in(argv[1]);
  if (!in) {
    fprintf(stderr, "ugui_golden: cannot read %s\n", argv[1]);
    return 2;
  }
  Writer w;
  w.f = fopen(argv[2], "wb");
  if (!w.f) {
    fprintf(stderr, "ugui_golden: cannot write %s\n", argv[2]);
    return 2;
  }
  w.bytes("UGGD", 4);
  w.u32v(1);

  RecordingTextures textures;
  textures.out = &w;

  UIContext ui;
  bool initialized = false;
  UIConfig config;
  config.draw_data = true;
  Vec2 window = {1280, 720}, framebuffer = {1280, 720};
  FontHandle font = kInvalidFont;
  uint32_t frame = 0;
  uint32_t last_atlas_revision = 0;
  bool atlas_seen = false;

  auto ensure_init = [&]() {
    if (initialized) return true;
    config.width = static_cast<i32>(window.x);
    config.height = static_cast<i32>(window.y);
    if (!ui.Init(config)) {
      fprintf(stderr, "ugui_golden: Init failed\n");
      return false;
    }
    // Lua 5.4 seeds math.random per state from the clock (luai_makeseed):
    // both runners seed it, so scripts that draw random numbers (neon,
    // glass, terminal) replay. A no-op without ULTRAGUI_LUA.
    ui.ExecScript("math.randomseed(42)");
    ui.set_texture_backend(&textures);
    host::SetViewport(*ui.platform(), window, framebuffer);
    initialized = true;
    return true;
  };

  auto render = [&](double dt) {
    ugui_golden_time += dt;
    const DrawData& dd = ui.RenderDrawData();
    TextEngine& te = ui.text_engine();
    const u32 rev = te.atlas_revision();
    if (!atlas_seen || rev != last_atlas_revision) {
      const Vec2 size = te.atlas_size();
      const u32 aw = static_cast<u32>(size.x), ah = static_cast<u32>(size.y);
      w.u8v('A');
      w.u32v(rev);
      w.u32v(aw);
      w.u32v(ah);
      const u8* px = te.atlas_pixels();
      w.u64v(px ? Fnv1a(px, size_t(aw) * ah) : 0);
      atlas_seen = true;
      last_atlas_revision = rev;
    }
    DumpFrame(w, frame++, dd);
  };

  std::string line;
  int lineno = 0;
  while (std::getline(in, line)) {
    ++lineno;
    const size_t hash = line.find('#');
    if (hash != std::string::npos) line.resize(hash);
    std::istringstream ls(line);
    std::string op;
    if (!(ls >> op)) continue;
    if (op == "viewport") {
      ls >> window.x >> window.y >> framebuffer.x >> framebuffer.y;
      if (initialized) host::SetViewport(*ui.platform(), window, framebuffer);
    } else if (op == "scale_mode") {
      std::string m;
      float dw, dh;
      ls >> m >> dw >> dh;
      config.scale_mode = m == "contain"  ? ViewportScaleMode::kContain
                          : m == "cover"  ? ViewportScaleMode::kCover
                          : m == "width"  ? ViewportScaleMode::kWidth
                          : m == "height" ? ViewportScaleMode::kHeight
                                          : ViewportScaleMode::kNone;
      config.design_width = dw;
      config.design_height = dh;
    } else if (op == "font") {
      std::string path;
      ls >> path;
      if (!ensure_init()) return 1;
      font = ui.LoadFont(path.c_str());
      if (font == kInvalidFont) {
        fprintf(stderr, "ugui_golden:%d: cannot load font %s\n", lineno,
                path.c_str());
        return 1;
      }
      ui.set_default_font(font);
    } else if (op == "ui") {
      std::string path;
      ls >> path;
      if (!ensure_init()) return 1;
      ui.LoadUi(path.c_str());
    } else if (op == "script") {
      std::string path;
      ls >> path;
      if (!ensure_init()) return 1;
      ui.LoadScript(path.c_str());
    } else if (op == "frame") {
      double dt = 1.0 / 60.0;
      ls >> dt;
      if (!ensure_init()) return 1;
      render(dt);
    } else if (op == "frames") {
      int n = 1;
      double dt = 1.0 / 60.0;
      ls >> n >> dt;
      if (!ensure_init()) return 1;
      for (int i = 0; i < n; ++i) render(dt);
    } else if (op == "move") {
      float x, y;
      ls >> x >> y;
      if (!ensure_init()) return 1;
      host::PushMouseMove(*ui.platform(), {x, y});
    } else if (op == "down" || op == "up") {
      std::string b = "left";
      ls >> b;
      if (!ensure_init()) return 1;
      host::PushMouseButton(*ui.platform(), ParseButton(b), op == "down");
    } else if (op == "scroll") {
      float x, y;
      ls >> x >> y;
      if (!ensure_init()) return 1;
      host::PushScroll(*ui.platform(), {x, y});
    } else if (op == "key") {
      int key, scancode, pressed, repeat, mods;
      ls >> key >> scancode >> pressed >> repeat >> mods;
      if (!ensure_init()) return 1;
      host::PushKey(*ui.platform(), key, scancode, pressed != 0, repeat != 0,
                    mods);
    } else if (op == "char") {
      unsigned cp;
      ls >> cp;
      if (!ensure_init()) return 1;
      host::PushChar(*ui.platform(), cp);
    } else if (op == "ui_scale") {
      float s;
      ls >> s;
      if (!ensure_init()) return 1;
      ui.set_ui_scale(s);
    } else {
      fprintf(stderr, "ugui_golden:%d: unknown op '%s'\n", lineno, op.c_str());
      return 2;
    }
  }
  w.u8v('E');
  if (initialized) ui.Shutdown();
  fclose(w.f);
  return 0;
}
