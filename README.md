# libultragui

Simple GPU-accelerated UI middleware for game engines. Vulkan rendering, flexbox layout via Yoga, text shaping via FreeType/HarfBuzz, Lua scripting. Ships as a static library.

## Why

Most game UI options boil down to: immediate mode (ImGui, good for tools, not so nice for complex logic or pretty custom game UI), embedded browser (Coherent, CEF, heavy), something old like scaleform widgets, or whatever your engine of choice ships.

libultragui is middleware, so it sits in the gap. Widgets are Retained-mode, with CSS-like styling, flexbox layout andproper text shaping, all rendering through your GPU. 
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

Interaction are done in Lua. Click `btn_inventory`, the runtime calls `on_btn_inventory`:

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

All container types (`std::vector`, `std::string`, `std::function`, etc.) are aliased through `core/config.h`. Point `ULTRAGUI_CUSTOM_CONFIG` at your own header to swap them for engine-native types:

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
| Rendering | Batched quads, SDF rounded rects, Vulkan | built-in |
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

## .ugui

`panel`, `text`, `button`, `image`, `scroll` widgets. Flexbox layout. `:hover`/`:pressed`/`:focused` selectors. `vw`/`vh`/`%`/`fr` units. Shadows, gradients, rounded corners, text transforms, transitions, keyframe animations.

The showcase has 8 scenes (dashboard, RPG journal, terminal, etc.), all `.ugui` + `.lua`, no C++.

## License

MIT
