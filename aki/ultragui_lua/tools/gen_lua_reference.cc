// Records what the C++ Lua runtime prints and does to the widgets for each
// scenario of tests/scenes_test.aki: the showcase's logic.lua and scenes, the
// game overlay HUD, and tests/data/api.{ugui,lua} (every ugui.* function,
// the audio and anim bindings, errors, timers and tweens). One file per
// scenario, tests/data/expected/<name>.txt: stdout (unbuffered) and stderr
// interleaved in program order, plus the driver's own lines (widget state
// dumps). tests/scenes_test.aki runs the same steps and must print the same
// bytes. Links the C++ library with ULTRAGUI_LUA=1. From the repository
// root, in the dev shell:
//
//   cmake -S . -B /tmp/ugui-ref-lua -G Ninja -DCMAKE_BUILD_TYPE=Release -DULTRAGUI_BUILD_EXAMPLES=OFF -DULTRAGUI_BUILD_TESTS=OFF
//   ninja -C /tmp/ugui-ref-lua ultragui
//   clang++ -std=c++20 -O2 -I. -Ithird_party/yoga $(pkg-config --cflags freetype2 harfbuzz lua5.4) \
//     aki/ultragui_lua/tools/gen_lua_reference.cc /tmp/ugui-ref-lua/libultragui.a \
//     /tmp/ugui-ref-lua/third_party/yoga/yoga/libyogacore.a /tmp/ugui-ref-lua/third_party/rlottie/librlottie.a \
//     $(pkg-config --libs freetype2 harfbuzz lua5.4) -lm -ldl -lpthread -o /tmp/gen_lua_reference
//   (cd aki/ultragui_lua && /tmp/gen_lua_reference tests/data/expected)
//
// Paths are relative to aki/ultragui_lua (the test's working directory).
// Keep the steps in sync with tests/scenes_test.aki.
#include <ugui/anim/vector_animation.h>
#include <ugui/audio/audio_backend.h>
#include <ugui/idl/builder.h>
#include <ugui/idl/parser.h>
#include <ugui/scripting/lua_anim.h>
#include <ugui/scripting/lua_audio.h>
#include <ugui/scripting/lua_widgets.h>
#include <ugui/scripting/script_runtime.h>
#include <ugui/widgets/button.h>
#include <ugui/widgets/checkbox.h>
#include <ugui/widgets/components.h>
#include <ugui/widgets/dropdown.h>
#include <ugui/widgets/image.h>
#include <ugui/widgets/slider.h>
#include <ugui/widgets/text.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

using namespace ugui;

static unsigned Bits(f32 v) {
  unsigned u;
  memcpy(&u, &v, 4);
  return u;
}

static std::string ColorText(Color c) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%08x,%08x,%08x,%08x", Bits(c.r), Bits(c.g),
           Bits(c.b), Bits(c.a));
  return buf;
}

// One line describing a widget's script-visible state.
static std::string DumpLine(World& world, wid w) {
  WidgetNode* n = world.Get<WidgetNode>(w);
  const Style& s = world.Get<StyleC>(w)->style;
  char buf[512];
  snprintf(buf, sizeof(buf),
           "%s k=%d op=%08x vis=%d fs=%08x cr=%08x,%08x,%08x,%08x,%08x "
           "sb=%08x cur=%d w=%d:%08x h=%d:%08x anim=%d",
           n->name.c_str(), static_cast<int>(n->kind), Bits(s.opacity),
           static_cast<int>(s.visibility), Bits(s.font_size),
           Bits(s.corner_radius), Bits(s.corner_radius_tl),
           Bits(s.corner_radius_tr), Bits(s.corner_radius_br),
           Bits(s.corner_radius_bl), Bits(s.shadow.blur),
           static_cast<int>(s.cursor), static_cast<int>(s.width.unit),
           Bits(s.width.value), static_cast<int>(s.height.unit),
           Bits(s.height.value), world.Has<AnimStyle>(w) ? 1 : 0);
  std::string line = buf;
  line += " tc=" + ColorText(s.text_color);
  line += " bg=" + ColorText(s.background);
  line += " sc=" + ColorText(s.shadow.color);
  line += " tsc=" + ColorText(s.text_shadow_color);
  if (TextContent* t = world.Get<TextContent>(w)) {
    line += " txt=\"" + std::string(t->text.c_str()) + "\"";
  }
  if (ButtonContent* b = world.Get<ButtonContent>(w)) {
    line += " lbl=\"" + std::string(b->label.c_str()) + "\"";
  }
  if (n->kind == WidgetKind::kCheckbox) {
    line += IsChecked(w) ? " chk=1" : " chk=0";
  }
  if (DropdownContent* d = world.Get<DropdownContent>(w)) {
    snprintf(buf, sizeof(buf), " sel=%d \"%s\"", d->selected,
             DropdownSelectedText(w).c_str());
    line += buf;
  }
  if (SliderContent* c = world.Get<SliderContent>(w)) {
    snprintf(buf, sizeof(buf), " val=%08x,%08x,%08x", Bits(c->value),
             Bits(c->min), Bits(c->max));
    line += buf;
  }
  if (ImageContent* c = world.Get<ImageContent>(w)) {
    snprintf(buf, sizeof(buf), " img=%u,%08x,%08x", c->texture,
             Bits(c->natural_w), Bits(c->natural_h));
    line += buf;
  }
  return line;
}

static void CollectNamed(World& world, wid w, std::vector<wid>& out) {
  if (!w.valid()) return;
  WidgetNode* n = world.Get<WidgetNode>(w);
  if (!n) return;
  if (!n->name.empty()) out.push_back(w);
  for (wid c : world.Get<Hierarchy>(w)->children) CollectNamed(world, c, out);
}

struct Tracker {
  std::vector<wid> widgets;
  std::vector<std::string> last;

  void Full(World& world) {
    last.clear();
    for (wid w : widgets) {
      last.push_back(DumpLine(world, w));
      printf("  %s\n", last.back().c_str());
    }
  }
  void Changes(World& world) {
    for (size_t i = 0; i < widgets.size(); ++i) {
      std::string line = DumpLine(world, widgets[i]);
      if (line != last[i]) {
        printf("  %s\n", line.c_str());
        last[i] = line;
      }
    }
  }
};

// Prints every call; handles count up from 1.
class RecordingAudio : public AudioBackend {
 public:
  bool Init() override { return true; }
  void Shutdown() override {}
  bool IsInitialized() const override { return true; }
  SoundHandle Play(const char* path, f32 volume, bool loop) override {
    printf("audio play %s %08x %d -> %u\n", path, Bits(volume), loop ? 1 : 0,
           next_);
    return next_++;
  }
  SoundHandle Load(const char* path) override {
    printf("audio load %s -> %u\n", path, next_);
    return next_++;
  }
  SoundHandle PlayLoaded(SoundHandle src, f32 volume, bool loop) override {
    printf("audio play_loaded %u %08x %d -> %u\n", src, Bits(volume),
           loop ? 1 : 0, next_);
    return next_++;
  }
  void Stop(SoundHandle h) override { printf("audio stop %u\n", h); }
  bool IsPlaying(SoundHandle h) const override {
    printf("audio is_playing %u\n", h);
    return h == 1;
  }
  void set_volume(SoundHandle h, f32 v) override {
    printf("audio set_volume %u %08x\n", h, Bits(v));
  }
  void set_pan(SoundHandle h, f32 v) override {
    printf("audio set_pan %u %08x\n", h, Bits(v));
  }
  void set_pitch(SoundHandle h, f32 v) override {
    printf("audio set_pitch %u %08x\n", h, Bits(v));
  }
  void Seek(SoundHandle h, f32 v) override {
    printf("audio seek %u %08x\n", h, Bits(v));
  }
  void set_master_volume(f32 v) override {
    printf("audio set_master_volume %08x\n", Bits(v));
    master_ = v;
  }
  f32 master_volume() const override {
    printf("audio master_volume\n");
    return master_;
  }
  void StopAll() override { printf("audio stop_all\n"); }
  void PauseAll() override { printf("audio pause_all\n"); }
  void ResumeAll() override { printf("audio resume_all\n"); }

 private:
  SoundHandle next_ = 1;
  f32 master_ = 1.0f;
};

static const double kUpdateTimes[] = {0.05, 0.1, 0.2, 0.25, 0.5,
                                      0.75, 1.0, 1.5, 2.0,  3.0};

static void RunScenario(const char* ugui_path, const char* lua_path,
                        bool api) {
  World& world = *WidgetRegistry::Active();
  UguiDocument doc;
  Vector<ParseError> errors;
  if (!ParseUguiFile(ugui_path, doc, errors)) {
    printf("parse failed\n");
    return;
  }
  UguiBuilder builder;
  wid root = builder.Build(doc);

  Tracker tracker;
  CollectNamed(world, root, tracker.widgets);

  ScriptRuntime rt;
  printf("init %d\n", rt.Init() ? 1 : 0);

  RecordingAudio audio;
  Vector<VectorAnimation*> anims;
  if (api) {
    rt.RegisterFunction("add", [](lua_State* L) -> int {
      lua_Number a = luaL_checknumber(L, 1);
      lua_Number b = luaL_checknumber(L, 2);
      lua_pushnumber(L, a + b);
      return 1;
    });
    RegisterAudioLua(rt, audio);
    RegisterAnimLua(
        rt,
        [&anims](const char* path, unsigned w, unsigned h) -> VectorAnimation* {
          printf("loader %s %u %u\n", path, w, h);
          auto* a = new VectorAnimation();
          if (!a->Load(nullptr, path, w, h)) {
            delete a;
            return nullptr;
          }
          anims.push_back(a);
          return a;
        },
        [&rt](const char* name) -> wid {
          printf("find_widget %s\n", name);
          return rt.FindRegisteredWidget(name);
        });
  }

  RegisterWidgetTree(rt, root);
  rt.WireChangeHandlers(root);
  rt.SyncTimerClock(0.0);
  // Lua 5.4 seeds math.random per state from the clock and an address.
  rt.Exec("math.randomseed(42)", "seed");
  if (api) {
    printf("setup %d\n",
           rt.Exec("ANIM_PATH = '../../examples/anim_demo/anims/spinner.uganim'",
                   "setup")
               ? 1
               : 0);
  }
  printf("exec %d\n", rt.ExecFile(lua_path) ? 1 : 0);
  printf("-- state\n");
  tracker.Full(world);

  for (wid w : tracker.widgets) {
    WidgetNode* n = world.Get<WidgetNode>(w);
    String name = n->name;
    if (n->kind == WidgetKind::kCheckbox) {
      printf("click %s\n", name.c_str());
      ClickWidget(world, w);
    } else if (n->kind == WidgetKind::kSlider) {
      printf("change %s\n", name.c_str());
      SliderContent* c = world.Get<SliderContent>(w);
      if (c && c->on_change) c->on_change(c->value);
    } else if (n->kind == WidgetKind::kDropdown) {
      printf("change %s\n", name.c_str());
      DropdownContent* c = world.Get<DropdownContent>(w);
      if (c && c->on_change) c->on_change(c->selected, DropdownSelectedText(w));
    }
    String handler = "on_" + name;
    printf("call %s %d\n", handler.c_str(),
           rt.CallHandler(handler.c_str(), w) ? 1 : 0);
    tracker.Changes(world);
  }

  for (double t : kUpdateTimes) {
    printf("update %.2f\n", t);
    rt.UpdateTimers(t);
    tracker.Changes(world);
  }

  if (api) {
    // Exec/ExecFile errors.
    printf("exec bad %d\n", rt.Exec("x(", "bad") ? 1 : 0);
    printf("exec boom %d\n", rt.Exec("error('boom')", "boom") ? 1 : 0);
    printf("exec default %d\n", rt.Exec("print('default chunk')", "chunk") ? 1 : 0);
    printf("execfile missing %d\n",
           rt.ExecFile("tests/data/missing.lua") ? 1 : 0);
    // A timer through the Lua-only ScheduleTimer.
    lua_State* L = rt.state();
    const char* src = "print('scheduled', ugui.get('box', 'opacity'))";
    luaL_loadbuffer(L, src, strlen(src), "sched");
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    rt.SyncTimerClock(4.0);
    rt.ScheduleTimer(0.5, ref);
    rt.Exec("ugui.after(0.25, function() print('after at 4.25') end)", "a");
    rt.Exec("ugui.tween('box', 'opacity', 1, 0, 1.0)", "t");
    printf("update 4.25\n");
    rt.UpdateTimers(4.25);
    tracker.Changes(world);
    printf("update 4.50\n");
    rt.UpdateTimers(4.5);
    tracker.Changes(world);
    // Pending timers and tweens are dropped.
    rt.Exec("ugui.after(1, function() print('never') end)", "n");
    rt.ClearTimersAndTweens();
    printf("update 9.00\n");
    rt.UpdateTimers(9.0);
    tracker.Changes(world);
    // Unregister, then a lookup misses.
    rt.UnregisterWidget(tracker.widgets[1]);
    rt.Exec("print('after unregister', ugui.find('title'))", "u");
    // RegisterFunction is a no-op once `ugui` is not a table.
    rt.Exec("saved = ugui; ugui = 5", "s");
    rt.RegisterFunction("zzz", [](lua_State*) -> int { return 0; });
    rt.Exec("ugui = saved; print('zzz', ugui.zzz)", "s2");
  }

  rt.ClearTimersAndTweens();
  rt.Shutdown();
  DestroyWidget(world, root);
  for (auto* a : anims) {
    a->Unload();
    delete a;
  }
}

struct Scenario {
  const char* name;
  const char* ugui;
  const char* lua;
  bool api;
};

static const Scenario kScenarios[] = {
    {"api", "tests/data/api.ugui", "tests/data/api.lua", true},
    {"showcase", "../../examples/showcase/ui.ugui",
     "../../examples/showcase/logic.lua", false},
    {"cosmos", "../../examples/showcase/scenes/cosmos.ugui",
     "../../examples/showcase/scenes/cosmos.lua", false},
    {"dashboard", "../../examples/showcase/scenes/dashboard.ugui",
     "../../examples/showcase/scenes/dashboard.lua", false},
    {"glass", "../../examples/showcase/scenes/glass.ugui",
     "../../examples/showcase/scenes/glass.lua", false},
    {"meridian", "../../examples/showcase/scenes/meridian.ugui",
     "../../examples/showcase/scenes/meridian.lua", false},
    {"neon", "../../examples/showcase/scenes/neon.ugui",
     "../../examples/showcase/scenes/neon.lua", false},
    {"nerv", "../../examples/showcase/scenes/nerv.ugui",
     "../../examples/showcase/scenes/nerv.lua", false},
    {"rpg_menu", "../../examples/showcase/scenes/rpg_menu.ugui",
     "../../examples/showcase/scenes/rpg_menu.lua", false},
    {"terminal", "../../examples/showcase/scenes/terminal.ugui",
     "../../examples/showcase/scenes/terminal.lua", false},
    {"game_overlay", "../../examples/game_overlay/hud.ugui",
     "../../examples/game_overlay/hud.lua", false},
};

int main(int argc, char** argv) {
  const char* out_dir = argc > 1 ? argv[1] : "tests/data/expected";
  setvbuf(stdout, nullptr, _IONBF, 0);
  int saved_out = dup(1);
  int saved_err = dup(2);
  for (const Scenario& s : kScenarios) {
    std::string path = std::string(out_dir) + "/" + s.name + ".txt";
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(fd, 1);
    dup2(fd, 2);
    close(fd);
    RunScenario(s.ugui, s.lua, s.api);
    dup2(saved_out, 1);
    dup2(saved_err, 2);
    fprintf(stderr, "wrote %s\n", path.c_str());
  }
  return 0;
}
