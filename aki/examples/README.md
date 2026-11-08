# The examples, in aki

Each directory is the aki port of `examples/<name>/main.cc`, an `exe`
package depending on the `ultragui*` packages it needs by path. Same command
lines, same behavior, same backends as the C++ (`examples/CMakeLists.txt`):

| Package | C++ program | Backend | Script runtime |
|---|---|---|---|
| `demo` | `ultragui_demo` | GlfwPlatform + VulkanRhi + Renderer2D, no UIContext | - |
| `showcase` | `ultragui_showcase [scene] [font]` | UIContext over GLFW + Vulkan, NERV render target | Lua (audio, Lottie, anim, video bindings) |
| `game_overlay` | `ultragui_game_overlay [font]` | UIContext over GLFW + Vulkan, paint callback | Lua (same) |
| `anim_demo` | `ultragui_anim_demo [font]` | UIContext over GLFW + Vulkan | NoScript |
| `lottie_demo` | `ultragui_lottie_demo [font]` | UIContext over GLFW + Vulkan, `ultragui_lottie` | NoScript |
| `video_demo` | `ultragui_video_demo [font]` | UIContext over GLFW + Vulkan, `ultragui_video` | NoScript |
| `embed_gl` | `ultragui_embed_gl` | host GL 3.3 context, UIContext embedded on OpenGlRhi | NoScript |
| `embed_opengl3` | `ultragui_embed_opengl3` | host GL, draw data through GlRenderer (ugui_impl_opengl3) | NoScript |
| `embed_vulkan` | `ultragui_embed_vulkan` | host Vulkan, draw data through VulkanBackend (ugui_impl_vulkan) | NoScript |
| `preview` | `ugui_preview <file.ugui> [--screenshot out.png] ...` | host GL, draw data through GlRenderer; hot reload | Lua (same) |

The C++ examples link the Lua runtime whatever they do; the ports that run
no script use `NoScript` (nothing observable changes). Where the C++ bakes
paths in at compile time (ULTRAGUI_SHADER_DIR, ULTRAGUI_*_DIR) the ports use
`@package_dir()`: the shaders are `ultragui_vulkan/shaders` (byte-identical
to the build's), the data files the C++ examples' own. Each `main.aki` lists
its shape differences in its header; the recurring ones are in
PORTING.md ("UIContext", "Known C++ quirks kept").

```
nix develop -c bash -c 'cd aki/examples/showcase && akiko build'      # -> build/ultragui_showcase
aki/examples/showcase/build/ultragui_showcase nerv /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
```

## Verification against the C++

Reference builds (Release; the default build has Vulkan, Lua, audio, Lottie,
video; embed_gl needs the OpenGL backend):

```
nix develop -c bash -c 'cmake -S . -B /tmp/ugui-ref-ex -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C /tmp/ugui-ref-ex'
nix develop -c bash -c 'cmake -S . -B /tmp/ugui-ref-gl -G Ninja -DCMAKE_BUILD_TYPE=Release -DULTRAGUI_BACKEND_OPENGL=ON \
    -DULTRAGUI_BACKEND_VULKAN=OFF -DULTRAGUI_BUILD_SHADERS=ON -DULTRAGUI_BUILD_TESTS=OFF && ninja -C /tmp/ugui-ref-gl'
```

Animated programs never show the same frame twice by wall clock, so they are
compared under `aki/tools/xshot_fixed.sh`, which preloads
`aki/tools/glfw_fixed_time.c`: `glfwGetTime` advances 1/60 s per
`glfwPollEvents` (one per frame) and stops at a given time, so both programs
get the same dt sequence and settle on the same frame; it also seeds Lua's
`math.random` (42), which Lua 5.4 otherwise seeds from the clock and
addresses. `XSHOT_KEYS` sends key presses and clicks (xdotool) after
`XSHOT_KEYS_AT` seconds, for scene switching and Lua click handlers.

```
F=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
aki/tools/xshot_fixed.sh /tmp/cpp.png 15 2.0 /tmp/ugui-ref-ex/examples/ultragui_showcase nerv $F
aki/tools/xshot_fixed.sh /tmp/aki.png 15 2.0 aki/examples/showcase/build/ultragui_showcase nerv $F
aki/tools/compare_shots.sh /tmp/aki.png /tmp/cpp.png                  # identical
XSHOT_KEYS="6 7" XSHOT_KEYS_AT=8 aki/tools/xshot_fixed.sh ...          # switch to nerv, then cosmos
```

Results (Xvfb, lavapipe for Vulkan, llvmpipe for GL; screenshots compared
with ImageMagick `compare -metric AE`):

| Example | Case | Result |
|---|---|---|
| demo | t = 1.5 s | pixel-identical |
| anim_demo | t = 1.3 s (mid-animation) | pixel-identical |
| lottie_demo | t = 1.3 s | pixel-identical |
| showcase | each of the 8 scenes from the command line, t = 2.0 s (neon, glass, terminal draw random numbers) | pixel-identical (8/8) |
| showcase | dashboard: clicks on a nav item, Acknowledge All, the Auto Deploy toggle (Lua handlers, a wired change handler) | pixel-identical |
| showcase | keys 6 then 7 (dashboard -> NERV render target -> cosmos, teardown); NERV started from the command line, then 1 | pixel-identical (2/2) |
| game_overlay | t = 2.0 s (cube, stars, HUD, the FPS line set from Lua) | pixel-identical |
| game_overlay | clicks on an ability and an item (Lua handlers set the notification) | pixel-identical |
| video_demo | scene 1, keys 2 and 3 (multi-view, the textured cube), t = 2.0 s | pixel-identical (3/3) |
| video_demo | Space, Right, L (pause, seek +2 s, loop off: "0:04 / 0:20  [PAUSED]") | pixel-identical |
| embed_gl | t = 2.0 s; a click on the button | pixel-identical |
| embed_opengl3 | t = 2.0 s | pixel-identical |
| embed_vulkan | t = 2.0 s | pixel-identical |
| preview | `--screenshot` of dashboard, neon (+ their Lua), game_overlay's hud, showcase's ui at 1024x640, 40 frames, `--bg #102030` | the PNG files are byte-identical (4/4) |
| preview | live mode: the .ugui touched -> "ugui_preview: reloaded ..." | works (aki checked) |
