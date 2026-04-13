/// Lottie animation demo: shows four Lottie animations side by side.
///
/// Usage: ./lottie_demo [font_path]

#include <ugui/ultragui.h>
#include <ugui/widgets/image.h>
#include <ugui/widgets/text.h>

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
  const char* label_name;
};

static const AnimInfo anims[] = {
    {"anims/spinner.json", "Spinner", "img_spinner", "lbl_spinner"},
    {"anims/pulse.json", "Pulse", "img_pulse", "lbl_pulse"},
    {"anims/bounce.json", "Bounce", "img_bounce", "lbl_bounce"},
    {"anims/wave.json", "Wave", "img_wave", "lbl_wave"},
};
static constexpr int ANIM_COUNT = 4;

// Layout built in code (no .ugui file needed)
static const char* LAYOUT = R"(
panel root {
    layout: column;
    width: 100vw;
    height: 100vh;
    padding: 40;
    gap: 30;
    background: #0f0f1a;
    align: center;

    text title {
        text: "Lottie Animation Demo";
        font-size: 28;
        color: #e0e0ff;
        text-align: center;
        letter-spacing: 2;
    }

    text subtitle {
        text: "Hand-crafted Lottie JSON rendered by rlottie";
        font-size: 14;
        color: #606080;
        text-align: center;
    }

    panel row {
        layout: row;
        justify: center;
        gap: 30;
        flex-grow: 1;
        align: center;

        panel card_spinner {
            layout: column;
            align: center;
            gap: 12;
            padding: 24;
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
            gap: 12;
            padding: 24;
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

        panel card_bounce {
            layout: column;
            align: center;
            gap: 12;
            padding: 24;
            background: #1a1a2e;
            corner-radius: 16;
            border-color: #2a2a50;
            border-width: 1;
            shadow-color: #00000040;
            shadow-blur: 12;
            shadow-y: 4;

            image img_bounce { width: 128; height: 128; }
            text lbl_bounce { text: "Bounce"; font-size: 13; color: #8888aa; text-align: center; }
        }

        panel card_wave {
            layout: column;
            align: center;
            gap: 12;
            padding: 24;
            background: #1a1a2e;
            corner-radius: 16;
            border-color: #2a2a50;
            border-width: 1;
            shadow-color: #00000040;
            shadow-blur: 12;
            shadow-y: 4;

            image img_wave { width: 128; height: 128; }
            text lbl_wave { text: "Wave"; font-size: 13; color: #8888aa; text-align: center; }
        }
    }

    text footer {
        text: "All animations are Lottie JSON files rendered on-the-fly via rlottie";
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
  config.title = "ultragui: Lottie Demo";
  config.width = 900;
  config.height = 520;
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

  // Load layout
  ui.LoadUiString(LAYOUT, "lottie_demo");

  // Load Lottie animations and attach to Image widgets
  std::string base = ULTRAGUI_LOTTIE_DEMO_DIR;
  ugui::LottieAnimation* loaded[ANIM_COUNT] = {};

  for (int i = 0; i < ANIM_COUNT; ++i) {
    std::string path = base + "/" + anims[i].file;
    auto* anim = ui.LoadLottie(path.c_str(), 128, 128);
    if (anim) {
      anim->Play();
      anim->set_loop(true);

      ugui::SetImageTexture(
          ui.widgets().Get(ui.FindWidget(anims[i].widget_name)),
          anim->texture(), 128, 128);

      loaded[i] = anim;
      std::printf("Loaded: %s (%.1fs, %u frames @ %.0f fps)\n", anims[i].label,
                  anim->duration(), anim->total_frames(), anim->frame_rate());
    } else {
      std::fprintf(stderr, "Failed to load: %s\n", path.c_str());
    }
  }

  // Main loop: UIContext auto-updates all Lottie animations
  while (ui.Running()) {
    // Re-attach textures each frame since the pixel data updates in-place
    // (the texture handle is stable, but the Image widget needs to know)
    for (int i = 0; i < ANIM_COUNT; ++i) {
      if (!loaded[i]) continue;
      ugui::SetImageTexture(
          ui.widgets().Get(ui.FindWidget(anims[i].widget_name)),
          loaded[i]->texture(), 128, 128);
    }

    ui.Update();
  }

  ui.Shutdown();
  return 0;
}
