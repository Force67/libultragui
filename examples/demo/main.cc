/// Demo: opens a window and renders colored rounded rectangles via the Vulkan
/// RHI.

#include <ugui/ultragui.h>

#include <cmath>
#include <cstdio>

int main() {
  ugui::Platform platform;

  ugui::Platform::WindowConfig wcfg;
  wcfg.width = 1280;
  wcfg.height = 720;
  wcfg.title = "libultragui demo";
#if ULTRAGUI_BACKEND_OPENGL
  wcfg.opengl = true;
#endif

  if (!platform.Init(wcfg)) {
    std::fprintf(stderr, "Failed to initialize platform\n");
    return 1;
  }

  ugui::RHI rhi;
  ugui::RHIConfig rcfg;
  rcfg.platform = &platform;
  rcfg.validation = true;
  rcfg.shader_dir = ULTRAGUI_SHADER_DIR;

  if (!rhi.Init(rcfg)) {
    std::fprintf(stderr, "Failed to initialize Vulkan RHI\n");
    platform.Shutdown();
    return 1;
  }

  ugui::Renderer2D renderer;
  renderer.Init(&rhi);

  std::printf("libultragui demo: rendering (%.0fx%.0f)\n", rhi.display_size().x,
              rhi.display_size().y);

  while (!platform.ShouldClose()) {
    platform.PollEvents();

    if (!rhi.BeginFrame(ugui::Color::FromHex(0x1a1a2e))) continue;

    renderer.BeginFrame();

    ugui::f32 t = static_cast<ugui::f32>(platform.time());

    // Background panel
    renderer.DrawRect({40, 40, 400, 640}, ugui::Color::FromHex(0x16213e, 0.9f),
                      16.0f);

    // Title bar
    renderer.DrawRect({40, 40, 400, 60}, ugui::Color::FromHex(0x0f3460), 16.0f);

    // Buttons
    for (int i = 0; i < 5; ++i) {
      ugui::f32 y = 140.0f + i * 80.0f;
      ugui::f32 pulse = 0.5f + 0.5f * std::sin(t * 2.0f + i * 0.5f);
      ugui::Color btn_color =
          ugui::Lerp(ugui::Color::FromHex(0x0f3460),
                     ugui::Color::FromHex(0xe94560), pulse * 0.3f);
      renderer.DrawRect({80, y, 320, 56}, btn_color, 8.0f);
    }

    // Floating card on the right
    ugui::f32 bounce = std::sin(t * 1.5f) * 20.0f;
    renderer.DrawRect({520, 100 + bounce, 700, 500},
                      ugui::Color::FromHex(0x533483, 0.85f), 24.0f);

    // Nested content
    renderer.DrawRect({560, 160 + bounce, 620, 120},
                      ugui::Color::FromHex(0xe94560, 0.6f), 12.0f);
    renderer.DrawRect({560, 310 + bounce, 300, 80},
                      ugui::Color::FromHex(0x0f3460, 0.8f), 8.0f);
    renderer.DrawRect({880, 310 + bounce, 300, 80},
                      ugui::Color::FromHex(0x0f3460, 0.8f), 8.0f);

    renderer.EndFrame();
    rhi.EndFrame();
  }

  renderer.Shutdown();
  rhi.Shutdown();
  platform.Shutdown();
  return 0;
}
