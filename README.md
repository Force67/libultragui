# libultragui

Simple GPU-accelerated UI middleware for game engines. Vulkan rendering (D3D11, D3D12 and GL 3.3 backends also available), flexbox layout via Yoga, text shaping via FreeType/HarfBuzz, Lua scripting. Ships as a static library.

## Why

Most game UI options boil down to: immediate mode (ImGui, good for tools, not so nice for complex logic or pretty custom game UI), embedded browser (Coherent, CEF, heavy), something old like scaleform widgets, or whatever your engine of choice ships.

libultragui is middleware, so it sits in the gap. Widgets are Retained-mode, with CSS-like styling, flexbox layout and proper text shaping, all rendering through your GPU.
Link it, hand it a window, call `Update()` once per frame.

## Authoring UI

Layouts live in `.ugui` files. Looks like CSS, works like CSS, but allows us to do a few shortcuts:

```
panel hud {
    layout: row;
    width: 100vw;
    padding: 20;
    gap: 12;
    background: #0f0f1a;

    text health_label {
        text: "HP";
        font-size: 14;
        color: #ff4466;
    }

    panel health_bar {
        width: 200;
        height: 8;
        background: #2a2a2a;
        corner-radius: 4;

        panel health_fill {
            width: 75%;
            height: 100%;
            background: #ff4466;
            corner-radius: 4;
        }
    }

    panel spacer { flex-grow: 1; }

    button btn_inventory {
        text: "Inventory";
        background: #4a4aff;
        corner-radius: 8;
        padding: 8 16;
        cursor: pointer;

        :hover { background: #5a5aff; }
        :pressed { background: #3a3aee; }
    }
}
```

Interaction is done in Lua. Click `btn_inventory`, the runtime calls `on_btn_inventory`:

```lua
function on_btn_inventory(widget)
    ugui.set("health_label", "text", "Opened inventory")
    ugui.set("health_label", "color", "#4aea8a")
end
```

Edit `.ugui`, edit `.lua`, restart. No build step.

## Integration

```cpp
ugui::UIConfig config;
config.title = "My Game";
config.width = 1920;
config.height = 1080;
config.shader_dir = ULTRAGUI_SHADER_DIR;

ugui::UIContext ui;
ui.Init(config);
ui.LoadFont("assets/ui/font.ttf");
// Or, for a font that lives in an archive or in the binary rather than on
// disk: ui.LoadFontMemory(bytes, length), which copies what it is given.
ui.set_default_font(font);
ui.LoadUi("assets/ui/hud.ugui");
ui.LoadScript("assets/ui/hud.lua");

while (running) {
    update_game();
    ui.Update(); // poll, layout, paint
}

ui.Shutdown();
```

`UIContext` is a convenience wrapper. If your engine already owns the window and render pass, use the layout engine, text shaper, and renderer as separate pieces.

### Custom allocators

All container types (`ugui::Vector`, `ugui::String`, `ugui::HashMap`, `ugui::Function`, etc.) are aliased through `core/config.h`, which documents the API each one must provide. By default they are the STL. `-DULTRAGUI_USE_BASE=ON` builds against equilibrium base instead, with no C++ standard library and no exceptions; set `ULTRAGUI_EQUILIBRIUM_DIR` to its checkout unless the parent project already defines `equilibrium::base`. Point `ULTRAGUI_CUSTOM_CONFIG` at your own header to swap them for engine-native types:

```cmake
add_definitions(-DULTRAGUI_CUSTOM_CONFIG="my_engine/ugui_types.h")
```

### Viewport scaling

Set a design resolution. The UI scales proportionally on resize:

```cpp
config.scale_mode = ugui::ViewportScaleMode::kContain;
config.design_width = 1920.0f;
config.design_height = 1080.0f;
```

## Features

| | What | Dependency |
|-|------|------------|
| Rendering | Batched quads, SDF rounded rects; Vulkan, D3D11, D3D12 or GL 3.3 | built-in |
| Layout | Flexbox (row, column, wrap, grow, gap, ...) | Yoga |
| Text | Glyph atlas, shaping, multi-weight/style | FreeType, HarfBuzz |
| Scripting | Lua bindings for the widget tree | Lua 5.4 |
| Audio | WAV/MP3/FLAC, panning, looping | miniaudio (optional) |
| SVG | CPU rasterizer, gradients, paths, transforms | built-in |
| Lottie | JSON animation playback | rlottie (optional) |
| Vector anim | `.uganim` keyframed shapes | built-in |

## Building

C++20. Nix for dependency management on linux/posix.

```bash
nix develop .#
cmake -B build -G Ninja
cmake --build build
./build/examples/ultragui_showcase   # press 1-8 to switch scenes
```

### Rendering backends

`ugui::RHI` has one implementation per graphics API, chosen at link time.
Vulkan is the default. `ULTRAGUI_BACKEND_D3D11`, `ULTRAGUI_BACKEND_D3D12` and
`ULTRAGUI_BACKEND_OPENGL` pick another one. `ULTRAGUI_RHI_SOURCE` overrides
all of them if you bring your own.

```bash
cmake -B build -G Ninja -DULTRAGUI_BACKEND_D3D11=ON
```

The D3D11 backend builds two ways from one source. On Windows it links
`d3d11` and `dxgi` from the platform SDK and takes the HWND behind the GLFW
window. Nothing else is needed there.

On Linux it links DXVK-native, which turns the same calls back into Vulkan.
Development and testing therefore need no Windows machine. That build wants
`dxvk-d3d11`, `dxvk-dxgi` and `libvkd3d-utils` on the pkg-config path, which
the dev shell provides. It also wants one environment variable, because
DXVK-native has no window system of its own and takes a `GLFWwindow*` in place
of an HWND:

```bash
export DXVK_WSI_DRIVER=GLFW
./build/examples/ultragui_showcase
```

D3D11 takes DXBC, which `dxc` no longer emits, so the backend compiles
`shaders/hlsl/*.hlsl` at startup. Windows uses `d3dcompiler_47.dll`, Linux
uses `D3DCompile` from vkd3d-utils, and the backend loads either one by name
at run time. CMake embeds that HLSL into the binary, so there are no shader
files to ship. D3D12 compiles the same sources to DXIL ahead of time instead.

All backends share one colour pipeline. The shaders decode vertex and texture
colours from sRGB, blending happens in linear light, and the sRGB render
target re-encodes on write. A colour written in `.ugui` therefore reaches the
screen unchanged. Offscreen render targets hold sRGB bytes like any other
texture. An sRGB view writes them and a UNORM view samples them, so the
hardware encodes once and the shader decodes once.

### Embedding in a host that owns everything (games, Windows)

When the host keeps its device and its window to itself, as a game hooked
at `Present` does, run ultragui in draw-data mode on the host platform:

```cmake
set(ULTRAGUI_PLATFORM_HOST ON)    # no window of its own; the host feeds it
set(ULTRAGUI_IMPL_DX11 ON)        # renders the draw list on the host's device
set(ULTRAGUI_BACKEND_VULKAN OFF)  # no RHI at all: a null one is linked
```

```cpp
ugui::UIConfig config;
config.draw_data = true;  // no external_window needed on the host platform
ui.Init(config);
ugui::dx11::Init(device, context);
ui.set_texture_backend(&ugui::dx11::texture_backend());

// each frame, with the host's render target bound:
ugui::host::SetViewport(*ui.platform(), size, size);
const ugui::DrawData& dd = ui.RenderDrawData();
if (ui.text_engine().atlas_revision() != font_revision) { /* UpdateFontAtlas */ }
ugui::dx11::RenderDrawData(dd);

// in the window procedure:
ugui::win32::HandleMessage(*ui.platform(), hwnd, msg, wparam, lparam, scale);
```

- `ugui/platform/platform_host.h`: the host sets the viewport and pushes
  input, from any thread; the context takes both when its frame starts.
  On Windows its clipboard is the system's.
- `ugui/backends/ugui_impl_dx11.h`: renders into whatever render target is
  bound and restores all the pipeline state it touched. On a render target
  that isn't sRGB, such as the usual game back buffer, it switches to shaders
  that encode sRGB themselves, so colours match.
- `ugui/backends/ugui_impl_win32.h`: Win32 mouse, wheel, key and character
  messages as ultragui input, with virtual keys mapped to the GLFW key codes
  the widgets read.

`ULTRAGUI_BUNDLED_DEPS`, which is on by default on Windows, builds freetype,
harfbuzz and Lua 5.4 from pinned release archives instead of asking
pkg-config. Offline, point `FETCHCONTENT_SOURCE_DIR_UGUI_FREETYPE` (and
`_HARFBUZZ`, `_LUA`) at unpacked copies of the same releases.

## .ugui

`panel`, `text`, `button`, `image`, `scroll` widgets. Flexbox layout. `:hover`/`:pressed`/`:focused` selectors. `vw`/`vh`/`%`/`fr` units. Shadows, gradients, rounded corners, text transforms, transitions, keyframe animations.

The showcase has 8 scenes (dashboard, RPG journal, terminal, etc.), all `.ugui` + `.lua`, no C++.

## License

MIT
