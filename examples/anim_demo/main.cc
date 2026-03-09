/// .uganim vector animation demo: lightweight alternative to Lottie.
///
/// Usage: ./anim_demo [font_path]

#include <ultragui/anim/vector_animation.h>
#include <ultragui/ultragui.h>
#include <ultragui/widgets/image.h>

#include <cstdio>
#include <cstdlib>
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

struct AnimInfo {
  const char* file;
  const char* label;
  const char* widget_name;
};

static const AnimInfo anims[] = {
    {"anims/spinner.uganim", "Spinner", "img_spinner"},
    {"anims/pulse.uganim", "Pulse", "img_pulse"},
    {"anims/checkmark.uganim", "Checkmark", "img_check"},
    {"anims/loading.uganim", "Loading", "img_loading"},
};
static constexpr int ANIM_COUNT = 4;

static const char* LAYOUT = R"(
panel root {
    layout: column;
    width: 100vw;
    height: 100vh;
    padding: 40;
    gap: 24;
    background: #0f0f1a;
    align: center;

    text title {
        text: ".uganim Vector Animation Demo";
        font-size: 28;
        color: #e0e0ff;
        text-align: center;
        letter-spacing: 2;
    }

    text subtitle {
        text: "1,181 LOC: zero dependencies: powered by the built-in SVG rasterizer";
        font-size: 14;
        color: #606080;
        text-align: center;
    }

    panel row {
        layout: row;
        justify: center;
        gap: 24;
        flex-grow: 1;
        align: center;

        panel card_spinner {
            layout: column;
            align: center;
            gap: 10;
            padding: 20;
            background: #1a1a2e;
            corner-radius: 16;
            border-color: #2a2a50;
            border-width: 1;
            shadow-color: #00000040;
            shadow-blur: 12;
            shadow-y: 4;

            image img_spinner { width: 128; height: 128; }
            text lbl_spinner { text: "Spinner"; font-size: 13; color: #8888aa; text-align: center; }
        }

        panel card_pulse {
            layout: column;
            align: center;
            gap: 10;
            padding: 20;
            background: #1a1a2e;
            corner-radius: 16;
            border-color: #2a2a50;
            border-width: 1;
            shadow-color: #00000040;
            shadow-blur: 12;
            shadow-y: 4;

            image img_pulse { width: 128; height: 128; }
            text lbl_pulse { text: "Pulse"; font-size: 13; color: #8888aa; text-align: center; }
        }

        panel card_check {
            layout: column;
            align: center;
            gap: 10;
            padding: 20;
            background: #1a1a2e;
            corner-radius: 16;
            border-color: #2a2a50;
            border-width: 1;
            shadow-color: #00000040;
            shadow-blur: 12;
            shadow-y: 4;

            image img_check { width: 128; height: 128; }
            text lbl_check { text: "Checkmark"; font-size: 13; color: #8888aa; text-align: center; }
        }

        panel card_loading {
            layout: column;
            align: center;
            gap: 10;
            padding: 20;
            background: #1a1a2e;
            corner-radius: 16;
            border-color: #2a2a50;
            border-width: 1;
            shadow-color: #00000040;
            shadow-blur: 12;
            shadow-y: 4;

            image img_loading { width: 128; height: 128; }
            text lbl_loading { text: "Loading Dots"; font-size: 13; color: #8888aa; text-align: center; }
        }
    }

    text footer {
        text: "All animations are .uganim JSON rendered via CPU rasterizer -> GPU texture";
        font-size: 12;
        color: #404060;
        text-align: center;
    }
}
)";

int main(int argc, char* argv[]) {
  const char* font_path = (argc > 1) ? argv[1] : find_font();
  if (!font_path) {
    std::fprintf(stderr, "No font found. Set ULTRAGUI_FONT or pass a path.\n");
    return 1;
  }

  ugui::UIConfig config;
  config.title = "ultragui: .uganim Demo";
  config.width = 900;
  config.height = 520;
  config.clear_color = ugui::Color::FromHex(0x0f0f1a);
  config.shader_dir = ULTRAGUI_SHADER_DIR;
  config.scale_mode = ugui::ViewportScaleMode::kContain;
  config.design_width = 900.0f;
  config.design_height = 520.0f;

  ugui::UIContext ui;
  if (!ui.Init(config)) return 1;

  auto font = ui.LoadFont(font_path);
  if (font == ugui::kInvalidFont) {
    ui.Shutdown();
    return 1;
  }
  ui.set_default_font(font);

  ui.LoadUiString(LAYOUT, "anim_demo");

  // Load .uganim animations and attach to Image widgets
  std::string base = ULTRAGUI_ANIM_DEMO_DIR;
  ugui::VectorAnimation* loaded[ANIM_COUNT] = {};

  for (int i = 0; i < ANIM_COUNT; ++i) {
    std::string path = base + "/" + anims[i].file;
    auto* anim = ui.LoadAnim(path.c_str(), 128, 128);
    if (anim) {
      anim->Play();
      anim->set_loop(true);

      auto* img =
          dynamic_cast<ugui::Image*>(ui.FindWidget(anims[i].widget_name));
      if (img) img->set_texture(anim->texture(), 128, 128);

      loaded[i] = anim;
      std::printf("Loaded: %s (%.1fs)\n", anims[i].label, anim->duration());
    } else {
      std::fprintf(stderr, "Failed to load: %s\n", path.c_str());
    }
  }

  while (ui.Running()) {
    // Re-attach textures each frame (handle is stable but Image needs to know)
    for (int i = 0; i < ANIM_COUNT; ++i) {
      if (!loaded[i]) continue;
      auto* img =
          dynamic_cast<ugui::Image*>(ui.FindWidget(anims[i].widget_name));
      if (img) img->set_texture(loaded[i]->texture(), 128, 128);
    }
    ui.Update();
  }

  ui.Shutdown();
  return 0;
}
