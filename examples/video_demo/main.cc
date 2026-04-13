/// Video playback demo: shows MPEG-1 video with GPU YCbCr->RGB conversion.
///
/// Scenes (press 1-3 to switch):
///   1. Basic video playback in a UI panel
///   2. Video texture reused across multiple widgets
///   3. Video on a spinning 3D cube (software-projected, GPU textured)
///
/// Controls:
///   Space : play/pause
///   R     : restart
///   L     : toggle loop
///   Left/Right: seek +-2s

#include <ugui/ultragui.h>
#include <ugui/widgets/image.h>
#include <ugui/widgets/text.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static const char* find_font() {
  const char* env = std::getenv("ULTRAGUI_FONT");
  if (env && fs::exists(env)) return env;
  FILE* pipe =
      popen("fc-match -f '%{file}' 'sans:style=Regular' 2>/dev/null", "r");
  if (pipe) {
    static char buf[1024];
    if (std::fgets(buf, sizeof(buf), pipe)) {
      auto len = std::strlen(buf);
      while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = 0;
      pclose(pipe);
      if (len > 0 && fs::exists(buf)) return buf;
    } else {
      pclose(pipe);
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Layouts
// ---------------------------------------------------------------------------

static const char* LAYOUT_BASIC = R"(
panel root {
    layout: column;
    width: 100vw; height: 100vh;
    padding: 30; gap: 20;
    background: #0f0f1a; align: center;

    text title { text: "MPEG-1 Video Player"; font-size: 26; color: #e0e0ff; text-align: center; letter-spacing: 2; }
    text subtitle { text: "GPU YCbCr->RGB conversion * pl_mpeg * zero licensing fees"; font-size: 13; color: #606080; text-align: center; }

    panel video_card {
        layout: column; align: center; padding: 12;
        background: #1a1a2e; corner-radius: 12;
        border-color: #2a2a50; border-width: 1;
        shadow-color: #00000060; shadow-blur: 20; shadow-y: 6;
        image video_frame { width: 320; height: 240; }
    }

    text time_display { text: "0:00 / 0:00"; font-size: 14; color: #8080a0; text-align: center; }
    text controls { text: "Space: play/pause  R: restart  L: loop  <-/->: seek"; font-size: 11; color: #404060; text-align: center; }
    text footer { text: "Press 1-3 to switch scenes"; font-size: 11; color: #303050; text-align: center; }
}
)";

static const char* LAYOUT_MULTI = R"(
panel root {
    layout: column;
    width: 100vw; height: 100vh;
    padding: 30; gap: 20;
    background: #0a0a18; align: center;

    text title { text: "Video Texture: One Decode, Many Widgets"; font-size: 24; color: #e0e0ff; text-align: center; }

    panel grid {
        layout: row; justify: center; gap: 20;
        flex-wrap: wrap; align: center; flex-grow: 1;

        panel card1 {
            layout: column; align: center; gap: 8; padding: 16;
            background: #1a1a2e; corner-radius: 16;
            border-color: #2a2a50; border-width: 1;
            shadow-color: #00000040; shadow-blur: 10; shadow-y: 4;
            image vid1 { width: 240; height: 180; }
            text lbl1 { text: "Original"; font-size: 12; color: #8888aa; text-align: center; }
        }
        panel card2 {
            layout: column; align: center; gap: 8; padding: 16;
            background: #1a1a2e; corner-radius: 16;
            border-color: #4a2a50; border-width: 1;
            shadow-color: #ff00aa20; shadow-blur: 10; shadow-y: 4;
            image vid2 { width: 160; height: 120; }
            text lbl2 { text: "Scaled Down"; font-size: 12; color: #aa88aa; text-align: center; }
        }
        panel card3 {
            layout: column; align: center; gap: 8; padding: 16;
            background: #1a1a2e; corner-radius: 16;
            border-color: #2a4a50; border-width: 1;
            shadow-color: #00aaff20; shadow-blur: 10; shadow-y: 4;
            image vid3 { width: 120; height: 120; }
            text lbl3 { text: "Square Crop"; font-size: 12; color: #88aaaa; text-align: center; }
        }
        panel card4 {
            layout: column; align: center; gap: 8; padding: 16;
            background: #1a1a2e; corner-radius: 16;
            border-color: #2a502a; border-width: 1;
            shadow-color: #00ff4420; shadow-blur: 10; shadow-y: 4;
            image vid4 { width: 80; height: 60; }
            text lbl4 { text: "Thumbnail"; font-size: 12; color: #88aa88; text-align: center; }
        }
    }

    text footer { text: "Same GPU texture: zero extra decode cost * Press 1-3 to switch"; font-size: 11; color: #404060; text-align: center; }
}
)";

static const char* LAYOUT_CUBE = R"(
panel root {
    layout: column;
    width: 100vw; height: 100vh;
    padding: 30; gap: 20;
    background: #050510; align: center;

    text title { text: "Video on 3D Cube"; font-size: 24; color: #e0e0ff; text-align: center; }
    text subtitle { text: "Software-projected cube faces, GPU-textured with decoded video"; font-size: 12; color: #505070; text-align: center; }
    panel spacer { flex-grow: 1; }
    text footer { text: "Press 1-3 to switch scenes"; font-size: 11; color: #303050; text-align: center; }
}
)";

// ---------------------------------------------------------------------------
// 3D cube: software projection, GPU-textured via RHI::DrawTriangles
// ---------------------------------------------------------------------------

struct Vec3 {
  float x, y, z;
};

static Vec3 rotate(Vec3 v, float yaw, float pitch) {
  // Rotate around Y axis
  float cy = std::cos(yaw), sy = std::sin(yaw);
  float x1 = v.x * cy + v.z * sy;
  float z1 = -v.x * sy + v.z * cy;
  // Rotate around X axis
  float cx = std::cos(pitch), sx = std::sin(pitch);
  float y2 = v.y * cx - z1 * sx;
  float z2 = v.y * sx + z1 * cx;
  return {x1, y2, z2};
}

static const Vec3 cube_verts[8] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
};

struct Face {
  int v[4];
};
static const Face faces[6] = {
    {{0, 1, 2, 3}}, {{5, 4, 7, 6}}, {{4, 0, 3, 7}},
    {{1, 5, 6, 2}}, {{3, 2, 6, 7}}, {{4, 5, 1, 0}},
};

static void draw_cube(ugui::RHI* rhi, ugui::RHITextureHandle tex, float cx,
                      float cy, float size, float time) {
  float yaw = time * 0.7f;
  float pitch = time * 0.4f;
  float fov = 3.0f;

  struct Proj {
    float x, y, z;
  };
  Proj proj[8];
  for (int i = 0; i < 8; i++) {
    Vec3 v = rotate(cube_verts[i], yaw, pitch);
    float d = fov / (fov + v.z + 2.0f);
    proj[i] = {cx + v.x * size * d, cy + v.y * size * d, v.z};
  }

  // Sort faces back-to-front
  struct FZ {
    int i;
    float z;
  };
  FZ sorted[6];
  for (int i = 0; i < 6; i++) {
    float az = 0;
    for (int j = 0; j < 4; j++) az += proj[faces[i].v[j]].z;
    sorted[i] = {i, az / 4.0f};
  }
  for (int i = 0; i < 5; i++)
    for (int j = i + 1; j < 6; j++)
      if (sorted[i].z > sorted[j].z) std::swap(sorted[i], sorted[j]);

  static const float uvs[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

  for (int fi = 0; fi < 6; fi++) {
    const auto& f = faces[sorted[fi].i];

    // Backface cull
    float ax = proj[f.v[1]].x - proj[f.v[0]].x;
    float ay = proj[f.v[1]].y - proj[f.v[0]].y;
    float bx = proj[f.v[2]].x - proj[f.v[0]].x;
    float by = proj[f.v[2]].y - proj[f.v[0]].y;
    if (ax * by - ay * bx < 0) continue;

    // Simple lighting from face normal Z
    Vec3 r0 = rotate(cube_verts[f.v[0]], yaw, pitch);
    Vec3 r1 = rotate(cube_verts[f.v[1]], yaw, pitch);
    Vec3 r2 = rotate(cube_verts[f.v[2]], yaw, pitch);
    float ex = r1.x - r0.x, ey = r1.y - r0.y, ez = r1.z - r0.z;
    float fx = r2.x - r0.x, fy = r2.y - r0.y, fz = r2.z - r0.z;
    float nz = ex * fy - ey * fx;
    float len = std::sqrt((ey * fz - ez * fy) * (ey * fz - ez * fy) +
                          (ez * fx - ex * fz) * (ez * fx - ex * fz) + nz * nz);
    float light = (len > 0) ? std::fabs(nz / len) : 0.5f;
    light = 0.3f + 0.7f * light;

    ugui::u8 shade = static_cast<ugui::u8>(light * 255);
    ugui::u32 col = (0xFFu << 24) | shade | (ugui::u32(shade) << 8) |
                    (ugui::u32(shade) << 16);

    ugui::Vertex2D verts[4] = {};
    for (int v = 0; v < 4; v++) {
      verts[v].pos[0] = proj[f.v[v]].x;
      verts[v].pos[1] = proj[f.v[v]].y;
      verts[v].uv[0] = uvs[v][0];
      verts[v].uv[1] = uvs[v][1];
      verts[v].color = col;
    }
    ugui::u32 idx[6] = {0, 1, 2, 0, 2, 3};
    rhi->DrawTriangles(verts, 4, idx, 6, tex);
  }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  const char* font_path = (argc > 1) ? argv[1] : find_font();
  if (!font_path) {
    std::fprintf(stderr, "No font found. Set ULTRAGUI_FONT or pass a path.\n");
    return 1;
  }

  std::string video_path = std::string(ULTRAGUI_VIDEO_DEMO_DIR) + "/demo.mpg";

  ugui::UIConfig config;
  config.title = "ultragui: Video Demo";
  config.width = 800;
  config.height = 600;
  config.clear_color = ugui::Color::FromHex(0x0f0f1a);
  config.shader_dir = ULTRAGUI_SHADER_DIR;

  ugui::UIContext ui;
  if (!ui.Init(config)) return 1;

  auto font = ui.LoadFont(font_path);
  if (font == ugui::kInvalidFont) {
    ui.Shutdown();
    return 1;
  }
  ui.set_default_font(font);

#if ULTRAGUI_VIDEO
  auto* video = ui.LoadVideo(video_path.c_str());
  if (!video) {
    std::fprintf(stderr, "Failed to load video: %s\n", video_path.c_str());
    ui.Shutdown();
    return 1;
  }
  video->Play();
  video->set_loop(true);

  std::printf("Video: %ux%u, %.1f fps, %.1fs duration\n", video->width(),
              video->height(), video->frame_rate(), video->duration());
#else
  std::fprintf(stderr, "ULTRAGUI_VIDEO not enabled!\n");
  ui.Shutdown();
  return 1;
#endif

  auto* window = static_cast<GLFWwindow*>(ui.platform()->native_handle());

  int scene = 0;
  bool scene_dirty = true;
  bool key_was_down[3] = {};

  while (ui.Running()) {
    // Scene switching (1/2/3)
    for (int i = 0; i < 3; i++) {
      bool down = glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS;
      if (down && !key_was_down[i] && scene != i) {
        scene = i;
        scene_dirty = true;
      }
      key_was_down[i] = down;
    }

    if (scene_dirty) {
      scene_dirty = false;
      switch (scene) {
        case 0:
          ui.LoadUiString(LAYOUT_BASIC, "basic");
          ui.set_clear_color(ugui::Color::FromHex(0x0f0f1a));
          ui.SetOnPaint({});
          break;
        case 1:
          ui.LoadUiString(LAYOUT_MULTI, "multi");
          ui.set_clear_color(ugui::Color::FromHex(0x0a0a18));
          ui.SetOnPaint({});
          break;
        case 2:
          ui.LoadUiString(LAYOUT_CUBE, "cube");
          ui.set_clear_color(ugui::Color::FromHex(0x050510));
          // Custom paint: draw the cube in the center, then the UI on top
          ui.SetOnPaint([&](ugui::Renderer2D& renderer, ugui::RHI* rhi) {
            auto vp = rhi->display_size();
            // Draw cube first (behind UI text)
            if (video->texture() != ugui::kInvalidTexture) {
              draw_cube(rhi, video->texture(), vp.x / 2.0f, vp.y / 2.0f,
                        std::min(vp.x, vp.y) * 0.22f,
                        static_cast<float>(ui.time()));
            }
            // Then draw the widget tree (title/footer text overlaid)
            if (ui.root()) {
              ugui::LayoutViewport lv{vp.x, vp.y, 1.0f};
              ugui::LayoutEngine le;
              ugui::Vector<ugui::LayoutNode> nodes;
              ugui::ComputeWidgetLayout(ui.root(), lv, le, nodes);
              ugui::PaintWidgetTree(ui.root(), renderer);
            }
          });
          break;
      }
    }

    // Playback controls
    static bool sp = false, rk = false, lk = false, lft = false, rgt = false;
    bool sp2 = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    bool rk2 = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    bool lk2 = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    bool lft2 = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
    bool rgt2 = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;

    if (sp2 && !sp) {
      if (video->IsPlaying())
        video->Pause();
      else
        video->Play();
    }
    if (rk2 && !rk) {
      video->Stop();
      video->Play();
    }
    if (lk2 && !lk) {
      video->set_loop(!video->IsLooping());
    }
    if (lft2 && !lft) {
      video->Seek(video->position() - 2.0);
    }
    if (rgt2 && !rgt) {
      video->Seek(video->position() + 2.0);
    }
    sp = sp2;
    rk = rk2;
    lk = lk2;
    lft = lft2;
    rgt = rgt2;

    // Attach video texture to Image widgets
    if (scene == 0) {
      if (video->texture() != ugui::kInvalidTexture)
        ugui::SetImageTexture(ui.widgets().Get(ui.FindWidget("video_frame")),
                              video->texture(),
                              static_cast<float>(video->width()),
                              static_cast<float>(video->height()));
      // Time display
      auto* td = ui.widgets().GetAs<ugui::Text>(ui.FindWidget("time_display"));
      if (td) {
        double pos = video->position(), dur = video->duration();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d:%02d / %d:%02d%s%s", (int)pos / 60,
                      (int)pos % 60, (int)dur / 60, (int)dur % 60,
                      video->IsLooping() ? "  [LOOP]" : "",
                      video->IsPlaying() ? "" : "  [PAUSED]");
        td->set_text(buf);
      }
    } else if (scene == 1) {
      const char* names[] = {"vid1", "vid2", "vid3", "vid4"};
      for (auto* name : names) {
        if (video->texture() != ugui::kInvalidTexture)
          ugui::SetImageTexture(ui.widgets().Get(ui.FindWidget(name)),
                                video->texture(),
                                static_cast<float>(video->width()),
                                static_cast<float>(video->height()));
      }
    }

    ui.Update();
  }

  ui.Shutdown();
  return 0;
}
