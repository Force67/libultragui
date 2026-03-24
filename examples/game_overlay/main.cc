/// Game overlay demo: renders a spinning 3D cube as "game content"
/// with a libultragui HUD overlaid on top.
///
/// Demonstrates that libultragui works as an overlay for game/3D applications:
/// the game draws its own scene, then the UI framework paints HUD elements
/// on top within the same render pass.

#include <ugui/layout/layout_tree.h>
#include <ugui/render/paint.h>
#include <ugui/ultragui.h>
#include <ugui/widgets/text.h>
#include <ugui/widgets/widget.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Font discovery (same helper as showcase)
// ---------------------------------------------------------------------------

static const char* find_font() {
  static std::string found;
  const char* env = std::getenv("ULTRAGUI_FONT");
  if (env && fs::exists(env)) return env;

  FILE* pipe =
      popen("fc-match -f '%{file}' 'sans:style=Regular' 2>/dev/null", "r");
  if (pipe) {
    char buf[1024];
    if (std::fgets(buf, sizeof(buf), pipe)) {
      std::string p(buf);
      while (!p.empty() && (p.back() == '\n' || p.back() == '\r')) p.pop_back();
      if (!p.empty() && fs::exists(p)) {
        pclose(pipe);
        found = p;
        return found.c_str();
      }
    }
    pclose(pipe);
  }

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

// ---------------------------------------------------------------------------
// Minimal 3D math (CPU-side projection for the spinning cube)
// ---------------------------------------------------------------------------

struct Vec3 {
  float x, y, z;
};

static Vec3 RotateY(Vec3 v, float a) {
  float c = std::cos(a), s = std::sin(a);
  return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

static Vec3 RotateX(Vec3 v, float a) {
  float c = std::cos(a), s = std::sin(a);
  return {v.x, v.y * c - v.z * s, v.y * s + v.z * c};
}

static std::pair<float, float> Project(Vec3 v, float screen_cx, float screen_cy,
                                       float focal) {
  float s = focal / (focal + v.z);
  return {screen_cx + v.x * s, screen_cy + v.y * s};
}

static Vec3 CrossProduct(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

static float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static Vec3 Normalize(Vec3 v) {
  float len = std::sqrt(Dot(v, v));
  if (len > 0) {
    v.x /= len;
    v.y /= len;
    v.z /= len;
  }
  return v;
}

// ---------------------------------------------------------------------------
// Cube definition
// ---------------------------------------------------------------------------

static constexpr Vec3 kCubeVerts[8] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
};

struct CubeFace {
  int idx[4];
  float base_r, base_g, base_b;
};

static const CubeFace kCubeFaces[6] = {
    {{0, 1, 2, 3}, 0.90f, 0.25f, 0.30f},  // front : red
    {{5, 4, 7, 6}, 0.20f, 0.60f, 0.95f},  // back  : blue
    {{4, 0, 3, 7}, 0.30f, 0.85f, 0.45f},  // left  : green
    {{1, 5, 6, 2}, 0.95f, 0.75f, 0.20f},  // right : yellow
    {{3, 2, 6, 7}, 0.85f, 0.40f, 0.90f},  // top   : purple
    {{4, 5, 1, 0}, 0.20f, 0.90f, 0.85f},  // bottom: cyan
};

static constexpr int kCubeEdges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
    {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

// ---------------------------------------------------------------------------
// Draw the spinning cube into the current render pass.
//
// IMPORTANT: All geometry is batched into a single vertex+index array and
// submitted with ONE DrawTriangles call. This is necessary because the RHI
// has a vertex dedup optimization that caches the pointer: calling
// DrawTriangles in a loop with a stack-local array would reuse the same
// address each iteration, causing the dedup to skip uploads and draw stale
// data for every face after the first.
// ---------------------------------------------------------------------------

static void DrawCube(ugui::RHI* rhi, float time, float screen_cx,
                     float screen_cy) {
  float scale = 120.0f;
  float focal = 600.0f;

  float rot_y = time * 0.7f;
  float rot_x = std::sin(time * 0.3f) * 0.35f + 0.4f;

  // Transform all 8 vertices
  Vec3 xformed[8];
  float proj_x[8], proj_y[8];
  for (int i = 0; i < 8; ++i) {
    Vec3 v{kCubeVerts[i].x * scale, kCubeVerts[i].y * scale,
           kCubeVerts[i].z * scale};
    v = RotateX(v, rot_x);
    v = RotateY(v, rot_y);
    xformed[i] = v;
    auto [px, py] = Project(v, screen_cx, screen_cy, focal);
    proj_x[i] = px;
    proj_y[i] = py;
  }

  // Sort ALL 6 faces back-to-front by average Z (painter's algorithm)
  struct FaceDraw {
    int face_idx;
    float avg_z;
  };
  FaceDraw draw_order[6];
  for (int f = 0; f < 6; ++f) {
    auto& face = kCubeFaces[f];
    draw_order[f] = {f, (xformed[face.idx[0]].z + xformed[face.idx[1]].z +
                         xformed[face.idx[2]].z + xformed[face.idx[3]].z) *
                            0.25f};
  }
  std::sort(
      draw_order, draw_order + 6,
      [](const FaceDraw& a, const FaceDraw& b) { return a.avg_z > b.avg_z; });

  Vec3 light_dir = Normalize({0.3f, -0.5f, -0.8f});

  // Batch all face + edge geometry into one vertex/index array.
  // 6 faces x 4 verts = 24, 12 edges x 4 verts = 48 -> max 72 verts
  // 6 faces x 6 idx  = 36, 12 edges x 6 idx  = 72 -> max 108 indices
  ugui::Vertex2D all_verts[72];
  ugui::u32 all_indices[108];
  ugui::u32 vert_count = 0;
  ugui::u32 idx_count = 0;

  // --- Faces (sorted back-to-front) ---
  for (int d = 0; d < 6; ++d) {
    auto& face = kCubeFaces[draw_order[d].face_idx];

    Vec3 e1{xformed[face.idx[1]].x - xformed[face.idx[0]].x,
            xformed[face.idx[1]].y - xformed[face.idx[0]].y,
            xformed[face.idx[1]].z - xformed[face.idx[0]].z};
    Vec3 e2{xformed[face.idx[2]].x - xformed[face.idx[0]].x,
            xformed[face.idx[2]].y - xformed[face.idx[0]].y,
            xformed[face.idx[2]].z - xformed[face.idx[0]].z};
    Vec3 n = Normalize(CrossProduct(e1, e2));
    n = {-n.x, -n.y, -n.z};  // flip inward -> outward

    float ndl = std::max(0.0f, Dot(n, light_dir));
    float light = 0.35f + 0.65f * ndl;

    ugui::u32 lit = ugui::Vertex2D::PackColor(
        face.base_r * light, face.base_g * light, face.base_b * light, 1.0f);

    ugui::u32 base = vert_count;
    for (int v = 0; v < 4; ++v) {
      int vi = face.idx[v];
      auto& vert = all_verts[vert_count++];
      vert = {};
      vert.pos[0] = proj_x[vi];
      vert.pos[1] = proj_y[vi];
      vert.uv[0] = (v == 1 || v == 2) ? 1.0f : 0.0f;
      vert.uv[1] = (v == 2 || v == 3) ? 1.0f : 0.0f;
      vert.color = lit;
      vert.color2 = lit;
    }

    all_indices[idx_count++] = base + 0;
    all_indices[idx_count++] = base + 1;
    all_indices[idx_count++] = base + 2;
    all_indices[idx_count++] = base + 0;
    all_indices[idx_count++] = base + 2;
    all_indices[idx_count++] = base + 3;
  }

  // --- Wireframe edges (on top of all faces) ---
  ugui::u32 edge_color = ugui::Vertex2D::PackColor(1.0f, 1.0f, 1.0f, 0.15f);
  float thickness = 1.2f;

  for (auto& edge : kCubeEdges) {
    float x0 = proj_x[edge[0]], y0 = proj_y[edge[0]];
    float x1 = proj_x[edge[1]], y1 = proj_y[edge[1]];

    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.01f) continue;
    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;

    ugui::u32 base = vert_count;
    auto& v0 = all_verts[vert_count++];
    v0 = {};
    v0.pos[0] = x0 + nx;
    v0.pos[1] = y0 + ny;
    v0.color = edge_color;
    v0.color2 = edge_color;
    auto& v1 = all_verts[vert_count++];
    v1 = {};
    v1.pos[0] = x1 + nx;
    v1.pos[1] = y1 + ny;
    v1.color = edge_color;
    v1.color2 = edge_color;
    auto& v2 = all_verts[vert_count++];
    v2 = {};
    v2.pos[0] = x1 - nx;
    v2.pos[1] = y1 - ny;
    v2.color = edge_color;
    v2.color2 = edge_color;
    auto& v3 = all_verts[vert_count++];
    v3 = {};
    v3.pos[0] = x0 - nx;
    v3.pos[1] = y0 - ny;
    v3.color = edge_color;
    v3.color2 = edge_color;

    all_indices[idx_count++] = base + 0;
    all_indices[idx_count++] = base + 1;
    all_indices[idx_count++] = base + 2;
    all_indices[idx_count++] = base + 0;
    all_indices[idx_count++] = base + 2;
    all_indices[idx_count++] = base + 3;
  }

  // Single draw call: avoids the RHI vertex dedup issue
  rhi->DrawTriangles(all_verts, vert_count, all_indices, idx_count);
}

// ---------------------------------------------------------------------------
// Draw a simple starfield background (via direct RHI calls, not renderer,
// to keep correct command buffer ordering with the cube).
// ---------------------------------------------------------------------------

static void DrawStars(ugui::RHI* rhi, float w, float h, float time) {
  static constexpr int kStarCount = 80;
  ugui::Vertex2D verts[kStarCount * 4];
  ugui::u32 indices[kStarCount * 6];

  for (int i = 0; i < kStarCount; ++i) {
    float fx = std::fmod(i * 127.1f + 311.7f, w);
    float fy = std::fmod(i * 269.5f + 183.3f, h);
    float b = 0.15f + 0.15f * std::sin(time * (0.5f + i * 0.03f) + i);
    float size = 1.0f + (i % 3) * 0.5f;

    ugui::u32 col = ugui::Vertex2D::PackColor(b, b, b * 1.2f, b);
    int vi = i * 4;
    verts[vi] = {};
    verts[vi].pos[0] = fx;
    verts[vi].pos[1] = fy;
    verts[vi].color = col;
    verts[vi].color2 = col;
    verts[vi + 1] = {};
    verts[vi + 1].pos[0] = fx + size;
    verts[vi + 1].pos[1] = fy;
    verts[vi + 1].color = col;
    verts[vi + 1].color2 = col;
    verts[vi + 2] = {};
    verts[vi + 2].pos[0] = fx + size;
    verts[vi + 2].pos[1] = fy + size;
    verts[vi + 2].color = col;
    verts[vi + 2].color2 = col;
    verts[vi + 3] = {};
    verts[vi + 3].pos[0] = fx;
    verts[vi + 3].pos[1] = fy + size;
    verts[vi + 3].color = col;
    verts[vi + 3].color2 = col;

    int ii = i * 6;
    ugui::u32 base = static_cast<ugui::u32>(vi);
    indices[ii] = base;
    indices[ii + 1] = base + 1;
    indices[ii + 2] = base + 2;
    indices[ii + 3] = base;
    indices[ii + 4] = base + 2;
    indices[ii + 5] = base + 3;
  }

  rhi->DrawTriangles(verts, kStarCount * 4, indices, kStarCount * 6);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  const char* font_path = (argc > 1) ? argv[1] : find_font();
  if (!font_path) {
    std::fprintf(stderr,
                 "No font found. Set ULTRAGUI_FONT or pass a TTF path.\n");
    return 1;
  }
  std::printf("Font: %s\n", font_path);

  ugui::UIConfig config;
  config.title = "libultragui: Game Overlay Demo";
  config.width = 1280;
  config.height = 720;
  config.clear_color = ugui::Color::FromHex(0x08081a);
  config.shader_dir = ULTRAGUI_SHADER_DIR;

  ugui::UIContext ui;
  if (!ui.Init(config)) {
    std::fprintf(stderr, "Failed to initialize\n");
    return 1;
  }

  auto font = ui.LoadFont(font_path);
  if (font == ugui::kInvalidFont) {
    ui.Shutdown();
    return 1;
  }
  ui.set_default_font(font);

  // Load HUD layout and scripting
  std::string base = ULTRAGUI_GAME_OVERLAY_DIR;
  ui.LoadUi((base + "/hud.ugui").c_str());
  ui.LoadScript((base + "/hud.lua").c_str());

  // Wire click handler for Lua
  ui.input().set_on_click([&ui](ugui::Widget* widget, ugui::MouseButton btn) {
    if (btn != ugui::MouseButton::kLeft || widget->name().empty()) return;
    std::string handler = "on_" + widget->name();
    ui.script().CallHandler(handler.c_str(), widget);
  });

  // Our own layout engine + scratch for painting the widget tree inside the
  // custom paint callback (UIContext's are private).
  ugui::LayoutEngine layout_engine;
  std::vector<ugui::LayoutNode> layout_scratch;

  int frame_count = 0;
  double fps_accum = 0.0;
  double last_fps_update = 0.0;

  ui.SetOnPaint([&](ugui::Renderer2D& r, ugui::RHI* rhi) {
    ugui::Vec2 ds = rhi->display_size();
    float t = static_cast<float>(ui.time());

    // --- "Game" content via direct RHI calls ---
    // Stars and cube use rhi->DrawTriangles directly (immediate mode)
    // so they appear in the correct order in the Vulkan command buffer.
    DrawStars(rhi, ds.x, ds.y, t);
    DrawCube(rhi, t, ds.x * 0.5f, ds.y * 0.45f);

    // --- HUD overlay via Renderer2D (batched) ---
    // Everything below is batched and flushed by the outer EndFrame,
    // so it draws ON TOP of the game content.
    r.DrawRectGradient({0, ds.y * 0.75f, ds.x, ds.y * 0.25f},
                       ugui::Color::FromHex(0x1a1a3a, 0.0f),
                       ugui::Color::FromHex(0x0a0a2a, 0.15f));

    ugui::Widget* root = ui.root();
    if (root) {
      ugui::LayoutViewport vp{ds.x, ds.y};
      ugui::ComputeWidgetLayout(root, vp, layout_engine, layout_scratch);
      ugui::PaintWidgetTree(root, r);
    }
  });

  std::printf(
      "Game overlay demo running. The 3D cube is the 'game'; HUD is "
      "libultragui.\n");

  while (ui.Running()) {
    // FPS tracking
    double now = ui.time();
    frame_count++;
    fps_accum += ui.delta_time();
    if (now - last_fps_update >= 0.5) {
      int fps = static_cast<int>(frame_count / fps_accum + 0.5);
      frame_count = 0;
      fps_accum = 0.0;
      last_fps_update = now;

      std::string fps_str = std::to_string(fps) + " FPS";
      if (!ui.script().Exec(
              ("ugui.set('fps_text', 'text', '" + fps_str + "')").c_str(),
              "fps"))
        if (auto* w = ui.FindWidget("fps_text"))
          dynamic_cast<ugui::Text*>(w)->set_text(fps_str.c_str());
    }

    ui.Update();
  }

  ui.Shutdown();
  return 0;
}
