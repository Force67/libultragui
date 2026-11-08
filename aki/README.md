# libultragui in Aki

This directory is a complete port of libultragui (the C++20 library under
`ugui/`) to Aki, a systems language whose compiler `akiko` (0.7.0) is on
`PATH` in the repository's nix dev shell. Every file of
`ugui/` has an Aki counterpart or a documented reason not to (macros and
containers that Aki has built in); the port keeps the C++'s behavior,
including its float operation order and its quirks, and checks that against
the C++ build rather than by inspection.

The structure follows the C++ build's options: **the core is one package, and
every optional backend and subsystem is its own package** that depends on it, so a
program links only what it uses (CMake's `ULTRAGUI_BACKEND_*`, `ULTRAGUI_LUA`,
`ULTRAGUI_LOTTIE`, ...). [`PORTING.md`](PORTING.md) is the detailed contract:
the module map from every C++ file to its Aki module, the idiom map, each seam,
the C++ quirks that were kept, the compiler pitfalls.

## Packages

| Package | What it is | Needs |
|---|---|---|
| `ultragui` | The core: style, animation, layout (Yoga), text (FreeType, HarfBuzz), widgets, input, SVG, vector animation, the `.ugui` parser and builder, `UIContext`, and the seams (`Rhi`, `Platform`, `ScriptRuntime`, `TextureBackend`, `AudioBackend`) | FreeType, HarfBuzz, libstdc++ (Yoga is compiled from `third_party/`) |
| `ultragui_glfw` | `GlfwPlatform` (window, input, clipboard, cursors) and the shared GLFW binding | GLFW |
| `ultragui_vulkan` | `VulkanRhi` (windowed) and `VulkanBackend` (draw-data renderer for a host that owns the device); the shared Vulkan binding (generated from `vulkan.h`) | Vulkan loader; GLFW |
| `ultragui_opengl` | `OpenGlRhi` and `GlRenderer` (GL 3.3 core, entry points loaded at run time) | a GL 3.3 context (via GLFW) |
| `ultragui_d3d11` | `D3d11Rhi`, `Dx11Renderer`, and the Win32 input backend | Windows: the SDK's d3d11/dxgi. Linux: DXVK-native (`DXVK_WSI_DRIVER=GLFW`) and vkd3d-utils |
| `ultragui_d3d12` | `D3d12Rhi` (vkd3d on Linux, presenting through a Vulkan swapchain, as the C++ does) | vkd3d, vkd3d-utils, Vulkan, GLFW |
| `ultragui_lua` | `LuaRuntime` (a `ScriptRuntime`) and the Lua animation/audio bindings | Lua 5.4 |
| `ultragui_lottie`, `ultragui_lua_lottie` | Lottie playback (rlottie, compiled from `third_party/`) and its Lua binding | nothing extra |
| `ultragui_video`, `ultragui_lua_video` | MPEG video (pl_mpeg, bundled) and its Lua binding | `ultragui_audio` |
| `ultragui_audio` | `AudioEngine`, the miniaudio backend (bundled) | the platform's audio libs |
| `ultragui_testing`, `ultragui_scenes` | Test support: the failure counters, and the scene the GL/D3D11 tests draw. Dev-dependencies, not part of the library | nothing |

`tools/` holds the verification scripts below, `tools/golden/` the draw-data
comparison with its runners (and `aki_golden`, an Aki program over the port).

Everything builds and runs inside the repository's dev shell, which supplies
`akiko`, clang, FreeType, HarfBuzz, GLFW, Vulkan, Lua, vkd3d, DXVK-native and
the rest, and puts them on `LD_LIBRARY_PATH` (the "Git tree is dirty" warning
is harmless):

```
nix develop -c bash -c 'cd aki/ultragui && akiko check && akiko test'
```

## Using it

Add the packages you need as path dependencies in your `aki.toml`:

```toml
[dependencies]
ultragui        = { path = "path/to/aki/ultragui" }
ultragui_glfw   = { path = "path/to/aki/ultragui_glfw" }
ultragui_vulkan = { path = "path/to/aki/ultragui_vulkan" }
ultragui_lua    = { path = "path/to/aki/ultragui_lua" }

[build]
features = ["ultragui_vulkan/embed_shaders"]   # SPIR-V compiled in; no shader_dir to ship
```

### An application: `UIContext` with GLFW and Vulkan

`UIContext<P, R, S>` is the C++ `UIContext` with its link-time choices (platform,
RHI, script runtime) as type parameters. Swap `VulkanRhi` for `OpenGlRhi`,
`D3d11Rhi` or `D3d12Rhi` (each from its package) and nothing else changes.

```aki
module game;

import {from_str} from akikit::strings::string;
import {UIConfig, UIContext} from ultragui::ui_context;
import {GlfwPlatform} from ultragui_glfw::platform_glfw;
import {VulkanRhi} from ultragui_vulkan::vulkan_rhi;
import {LuaRuntime} from ultragui_lua::lua_runtime;   // or NoScript from ultragui::scripting::script_runtime

pub fn main(): i32 {
  var ui = UIContext::<GlfwPlatform, VulkanRhi, LuaRuntime>(GlfwPlatform(), VulkanRhi(), LuaRuntime());
  if (!ui.init(UIConfig { title: from_str("My Game"), width: 1920, height: 1080 })) {
    return 1;
  }
  ui.set_default_font(ui.load_font("assets/ui/font.ttf"));
  ui.load_ui("assets/ui/hud.ugui");
  ui.load_script("assets/ui/hud.lua");   // Lua: on_<widget>(widget) handlers, the ugui.* API
  ui.set_script_click_dispatch();        // a left click on a named widget calls on_<name>
  while (ui.running()) {
    ui.update();                         // poll, layout, paint
  }
  ui.shutdown();
  return 0;
}
```

This program, with `terminal.ugui`/`terminal.lua` from `examples/showcase` in
place of the HUD, draws the showcase's terminal scene. Without a display, `aki/tools/xshot.sh out.png 6 ./build/game` runs it under Xvfb on
Mesa's software Vulkan and screenshots it.

### Draw-data mode: your renderer draws the UI

When the host owns the window and the GPU (a game hooked at `Present`, an
engine with its own render pass), run the UI with no device of its own. The
host feeds window size and input; `render_draw_data()` returns quads, text
vertices and commands for the host's renderer, which also owns textures and
the glyph atlas. `HostPlatform` is the platform that is fed, `NullRhi` the
RHI that does nothing:

```aki
import {Vec2} from ultragui::core::math;
import {TextureBackend, TextureBackendBox} from ultragui::render::texture_backend;
import {NullRhi} from ultragui::rhi::null_rhi;
import {HostPlatform} from ultragui::platform::platform_host;
import {NoScript} from ultragui::scripting::script_runtime;
import {UIConfig, UIContext} from ultragui::ui_context;

// HostTextures implements TextureBackend: create_texture, update_texture, destroy_texture.
var ui = UIContext::<HostPlatform, NullRhi, NoScript>(HostPlatform(), NullRhi(), NoScript());
ui.init(UIConfig { draw_data: true, width: 1280, height: 720 });
ui.set_texture_backend(TextureBackendBox::of(HostTextures {}));
ui.set_default_font(ui.load_font("font.ttf"));
ui.load_ui("hud.ugui");

// each frame:
ui.platform.set_viewport(window_size, framebuffer_size);
ui.platform.push_mouse_move(Vec2 { x, y });               // push_mouse_button, push_scroll, push_key, push_char
let dd = ui.render_draw_data();                           // dd.quad_vertices, dd.text_vertices, dd.commands, ...
// re-upload ui.text_engine.atlas_pixels() when ui.text_engine.atlas_revision() changed, then draw dd
```

The packages that draw it for you: `VulkanBackend` (`ultragui_vulkan::ugui_impl_vulkan`),
`GlRenderer` (`ultragui_opengl::ugui_impl_opengl3`) and `Dx11Renderer`
(`ultragui_d3d11::ugui_impl_dx11`), each the C++ `ugui_impl_*` for its API.
Full programs: `ultragui_vulkan/examples/embed_scene` (a host that owns the
Vulkan device) and `tools/golden/aki_golden` (draw data into a recording sink).

### Lua

Lua scripts are the same as the C++ library's: the `ugui.*` functions
(`ugui.set`, `ugui.log`, ...), handlers named `on_<widget>`, the animation and
audio functions. Pick the runtime by the
third type parameter of `UIContext`: `NoScript` (no Lua, no dependency) or
`LuaRuntime` (`ultragui_lua`). Lottie and video playback in Lua come from
`ultragui_lua_lottie` and `ultragui_lua_video`, which register their functions
with the runtime.

## Building and testing

One script checks and tests every package, and prints a summary:

```
aki/tools/test_all.sh                       # everything
aki/tools/test_all.sh ultragui_d3d12 ...    # only these packages
```

For each directory with an `aki.toml` (the packages, the examples, the golden
runner) it runs `akiko check`, `akiko check --target x86_64-w64-windows-gnu`
(every package passes it; the Windows-only code is type-checked, though never
run) and `akiko test`. The tests run under Xvfb on Mesa's software renderers
(lavapipe for Vulkan, and through it DXVK-native and vkd3d; llvmpipe for GL),
so the windowed backends' tests really draw instead of skipping. Logs are in
`/tmp/aki-test-all`; the exit status is 1 if anything failed.

By hand, for one package:

```
nix develop -c bash -c 'cd aki/ultragui_vulkan && akiko check && akiko test'
aki/tools/test_xvfb.sh ultragui_d3d12                 # a package's tests with a software GPU and a display
aki/tools/test_xvfb.sh ultragui_d3d12 tests/rhi_test.aki
```

The C++ library builds and tests the usual way (see the top-level README):

```
nix develop -c bash -c 'cmake -S . -B /tmp/ugui-ref -G Ninja && ninja -C /tmp/ugui-ref && ctest --test-dir /tmp/ugui-ref'
```

## How it is verified

The port is right when it does what the C++ does, so each layer is compared
with the C++ itself, in three ways:

1. **C++-recorded references.** The numeric core (math, colors, styles,
   transitions, text shaping, layout, paint, renderer vertices, SVG and
   vector-animation rasterization, the Lua scripting surface, Lottie and video)
   is tested against values the C++ printed: floats as bit patterns, pixels as
   hashes. Each package's `tools/gen_*reference*.cc` (build line in its header)
   regenerates them from the C++, e.g. `aki/ultragui/tools/gen_reference.cc`.
   The SVG rasterizer is byte-identical on a 62-render corpus, vector
   animation on 330 frames, the `.ugui` parser's AST on every example UI plus a
   25-file corpus.
2. **Golden draw-data comparison.** Both implementations play the same
   scenario (a `.ugui`, optionally its Lua, and a seeded stream of input) in
   draw-data mode with a fixed clock, and dump every frame: vertex and index
   buffers, draw commands, texture uploads, atlas changes. `compare.py` names
   the first frame, buffer and field that differ. The example UIs agree (the
   `_lua` scenarios need the Lua runtime named in the aki runner).
   See [`tools/golden/README.md`](tools/golden/README.md):

   ```
   nix develop -c bash -c 'cmake -S aki/tools/golden -B /tmp/ugui-golden -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C /tmp/ugui-golden'
   nix develop -c bash -c 'cd aki/tools/golden/aki_golden && akiko build'
   nix develop -c aki/tools/golden/run.sh /tmp/ugui-golden/ugui_golden aki/tools/golden/aki_golden/build/aki_golden \
       $(ls aki/tools/golden/scenarios/*.txt | grep -v _lua) aki/tools/golden/extra/*.txt
   ```
3. **Pixel-identical screenshots of the GPU backends.** For each RHI, a C++
   program and an Aki program draw the same scene through the same calls; both run
   under `aki/tools/xshot.sh` (Xvfb with a software GPU) and the screenshots are
   compared with `aki/tools/compare_shots.sh a.png b.png`, which must report
   `identical`. The C++ twins are `ultragui_vulkan/examples/{rhi_scene,embed_scene}/*.cc`,
   `ultragui_d3d12/tools/rhi_scene.cc` and `tools/{rhi_scene,draw_data_scene,draw_data_scene_dx11}.cc`;
   each file's header has its build line, and the per-backend recipes are in
   PORTING.md ("Backend packages", "ultragui_d3d12", ...). For example, D3D12:

   ```
   nix develop -c bash -c 'cmake -S . -B /tmp/ugui-d3d12-ref -G Ninja -DULTRAGUI_BACKEND_D3D12=ON -DULTRAGUI_BACKEND_VULKAN=OFF -DULTRAGUI_BUILD_EXAMPLES=OFF && ninja -C /tmp/ugui-d3d12-ref ultragui ultragui_hlsl_shaders'
   # build rhi_scene.cc as in its header -> /tmp/d3d12_scene_cc
   nix develop -c bash -c 'cd aki/ultragui_d3d12/examples/rhi_scene && akiko build'
   aki/tools/xshot.sh /tmp/cpp.png 12 /tmp/d3d12_scene_cc
   aki/tools/xshot.sh /tmp/aki.png 12 aki/ultragui_d3d12/examples/rhi_scene/build/rhi_scene
   aki/tools/compare_shots.sh /tmp/aki.png /tmp/cpp.png
   ```

## Differences from the C++

Behavior is meant to be identical; shapes differ where Aki's types make the C++
shape impossible or unidiomatic.

* **What is kept on purpose**: the C++'s quirks, including the ones that look like
  bugs, are reproduced, so that output matches. They are listed in PORTING.md
  under [Known C++ quirks kept](PORTING.md#known-c-quirks-kept) (with the three
  endless C++ loops in the SVG parser that terminate in the port, the one place
  behavior is deliberately not the same).
* **What is shaped differently**: backends are generic type parameters of
  `UIContext` instead of link-time switches; pointers become views, optionals
  and results; C++ overloads become distinct names (`load_font_styled`);
  subsystems that stored a backend pointer (Lottie, video, vector animations)
  take it per call. See [Shape differences](PORTING.md#shape-differences-not-behavior),
  the seams ([Seams](PORTING.md#seams)) and the
  [naming and idiom maps](PORTING.md#idiom-map-the-concrete-types-chosen).
* **Known limits**: the bit-exact tables for float results were recorded on
  aarch64 (where clang fuses multiply-adds, as `akiko` always does); the C++
  on x86-64 does not fuse. The D3D12 backend runs on Linux (vkd3d) only, as
  the C++ backend does (it has no DXGI swapchain); the Windows halves of the
  D3D11 and Win32 code type-check but were never run. The C++
  `ugui_impl_dx11.cc` does not compile as it stands, so its reference is built from
  a patched copy (`tools/build_d3d11_refs.sh`). `lua_video`'s test is Aki-only
  (the C++ player needs a live RHI).
