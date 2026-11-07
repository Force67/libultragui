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

The first line that needs a context initializes it.

## The C++ side

`ugui_golden.cc` documents the dump format. `golden_platform.cc` is
`ugui/platform/platform_host.cc` with `Platform::time()` reading the
scenario's clock; `gen_platform.py` regenerates it when the host platform
changes.
