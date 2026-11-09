# Golden comparison: the aki port against the C++ library

The port is right when it hands a renderer what the C++ hands it. Both sides
play the same scenario in draw-data mode on the host platform, with a clock the
scenario sets, and write every frame to a dump: the quad and text vertex and
index buffers, the draw commands, every texture upload (size, format, and a
hash of the pixels), and the glyph atlas whenever it changes. `compare.py`
diffs two dumps and names the first frame, buffer, element and field that
differ.

```
nix develop -c bash -c '
  cmake -S aki/tools/golden -B /tmp/ugui-golden -G Ninja -DCMAKE_BUILD_TYPE=Release &&
  ninja -C /tmp/ugui-golden &&
  aki/tools/golden/run.sh /tmp/ugui-golden/ugui_golden <aki runner>'
```

`run.sh` plays every scenario in `scenarios/` through both runners from the
repository root and compares the dumps. The aki runner is
`aki/tools/golden/aki_golden` (an aki program over the port with the same
command line: `<scenario> <out.bin>`).

## Scenarios

`gen_scenarios.py` writes `scenarios/` from the example UIs: each `.ugui`
alone, and with its Lua script where it has one. A seeded walk of input follows
a few settling frames, so the files only change when the script does. One
operation per line; `#` starts a comment:

| Line | Effect |
|------|--------|
| `viewport W H FW FH` | window and framebuffer size (before init: the config size) |
| `scale_mode none\|width\|height\|contain\|cover DW DH` | viewport scaling, before init |
| `font PATH` | load a font and make it the default |
| `ui PATH` | load a `.ugui` |
| `script PATH` | load a Lua script |
| `frame [DT]` | advance the clock by DT (default 1/60 s), render, dump |
| `frames N DT` | that N times |
| `move X Y`, `down B`, `up B` | pointer, B = left, right or middle |
| `scroll DX DY` | wheel |
| `key KEY SCANCODE PRESSED REPEAT MODS` | a GLFW key event |
| `char CODEPOINT` | text input |
| `ui_scale S` | `set_ui_scale(S)` |

The first line that needs a context initializes it. Left clicks on named
widgets dispatch to Lua's `on_<name>` handler, as in the showcase and game
overlay examples.

## Extra scenarios

`extra/` holds hand-written scenarios for what the example UIs do not
exercise: `media_scale.txt` (with `media_scale.ugui`) covers `scale_mode`,
`ui_scale`, `@media` breakpoints across resizes and tooltip dwell. `run.sh`
takes them as arguments: `run.sh <c++> <aki> aki/tools/golden/extra/*.txt`.

## The aki side

`aki_golden/` is the aki runner (`cd aki/tools/golden/aki_golden && akiko
build` -> `build/aki_golden`, built at -O2). Its script runtime is the one
alias `Runtime` in `src/main.aki`: ultragui_lua's `LuaRuntime`, with the
bindings of the C++ runner's build (`add_core_lua(ui, LuaBindings { audio:
false })`: ULTRAGUI_LUA on, ULTRAGUI_AUDIO, LOTTIE and VIDEO off, so only
RegisterAnimLua). All 20 scenarios and the extra ones compare `same`.

## Random numbers

Lua 5.4 seeds `math.random` per state from the clock and an address
(`luai_makeseed`), so a script that draws random numbers (neon, glass,
terminal) would not replay, not even against itself. Both runners run
`math.randomseed(42)` right after Init (`ExecScript` / `exec_script`, before
the texture backend and viewport are set); without ULTRAGUI_LUA it is a
no-op.

## The C++ side

Both runners fail if a requested UI or script cannot be loaded or executed.
The end marker follows shutdown, so texture destruction is included in the
comparison. `compare.py` requires a complete version-1 dump with no bytes
after the end marker and compares float bit patterns, including signed zero.

Regression tests for the harness itself:

```
python3 -m unittest discover -s aki/tools/golden -p test_compare.py
nix develop -c python3 aki/tools/golden/test_runners.py /tmp/ugui-golden/ugui_golden aki/tools/golden/aki_golden/build/aki_golden
```

`ugui_golden.cc` documents the dump format. `golden_platform.cc` is
`ugui/platform/platform_host.cc` with `Platform::time()` reading the
scenario's clock; `gen_platform.py` regenerates it when the host platform
changes.
