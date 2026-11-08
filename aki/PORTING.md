# Porting libultragui to aki: the contract

`ugui/` (C++20) is being ported to aki, module by module, behavior for
behavior. This file is the contract every porting agent works to: where each
C++ file goes, how C++ idioms map to the aki types chosen here, every seam
with its signature, the commands, and the compiler limits found so far. Read
it before writing a module; when you decide something the next module will
depend on, write it down here in the same commit.

## Layout and commands

```
aki/
  PORTING.md             this file
  ultragui/              package `ultragui` (kind lib): the core
    aki.toml             yoga from third_party via c_sources; -lfreetype -lharfbuzz
    src/<dir>/<file>.aki ugui/<dir>/<file>.{h,cc}  ->  module ultragui::<dir>::<file>
    src/sys/             hand-written C bindings (freetype, harfbuzz, c_long)
    os/<os>/ultragui/sys/c_long.aki   C `long` per target OS (CLong, CULong)
    tests/*_test.aki     one program per test; tests/support/ shared helpers
    tools/gen_reference.cc  prints C++-computed reference values for the tests
    tools/gen_anim_reference.cc, gen_ugui_ast.sh (+ dump_ugui.cc, gen_*_fuzz.py)  anim and .ugui parser references
    tools/gen_svg_reference.cc, gen_svg_table.py, gen_svg_corpus.py
                         SVG pixel hashes from the C++ -> tests/support/svg_corpus.aki
    tests/data/svg/      the SVG corpus (@embed'ed by tests/support/svg_corpus.aki)
  ultragui_audio/        miniaudio AudioBackend (csrc/ugui_ma.c shim)
  ultragui_lottie/       rlottie from third_party via c_sources; tools/gen_reference.cc + gen_tables.py
  ultragui_video/        pl_mpeg via [build.implement]; tools/{gen_reference,recording_rhi}.cc + gen_tables.py
  tools/golden/          draw-data golden harness (C++ side), see its README
  tools/xshot.sh         screenshots of windowed backends under Xvfb
```

Backends and optional subsystems become their own packages beside
`ultragui/`, each depending on it: `ultragui_glfw`, `ultragui_vulkan`,
`ultragui_opengl`, `ultragui_d3d11`, `ultragui_d3d12`, `ultragui_lua`,
`ultragui_lottie`, `ultragui_video`, `ultragui_audio`, `ultragui_win32`.

All commands run in the repository's dev shell (it supplies clang, FreeType,
HarfBuzz and libstdc++, and puts them on `LD_LIBRARY_PATH` for programs akiko
builds; "Git tree is dirty" is harmless):

```
nix develop -c bash -c 'cd aki/ultragui && akiko check'          # package + tests
nix develop -c bash -c 'cd aki/ultragui && akiko test'           # build and run tests/*_test.aki
nix develop -c bash -c 'cd aki/ultragui && akiko test tests/world_test.aki'
nix develop -c bash -c 'cd aki/ultragui && akiko check --target x86_64-w64-windows-gnu'  # Windows layouts
```

The C++ reference library and its tests:

```
nix develop -c bash -c 'cmake -S . -B /tmp/ugui-ref -G Ninja && ninja -C /tmp/ugui-ref && ctest --test-dir /tmp/ugui-ref'
```

Reference values for bit-exact tests come from `aki/ultragui/tools/gen_reference.cc`
(its header has the build line); Renderer2D's from `tools/gen_renderer_reference.cc`,
layout + paint's from `tools/gen_paint_reference.cc` (links a Release build of the
C++ library: at -O0 clang does not contract `a * b + c`, akiko always does). Extend it with a function per subsystem,
print one tagged line per value (`TAG a b c`, floats as `0x%08x` bits), and
paste the lines into the test as `static let` tables. The tables in
`core_test.aki` and `text_engine_test.aki` were recorded on aarch64 Linux.

Commit early and in logical steps; a module, its test, and any contract
change here go together.

## Module map

Status: **ported** (done, tested), **partial** (what is missing is named),
**next** (core package, next wave), **pkg:<name>** (backend/optional
package), **gone** (no counterpart, reason given).

| C++ | aki | Status |
|---|---|---|
| core/algorithm.h | ultragui::core::algorithm | ported (IntroSort is libstdc++'s std::sort step for step) |
| core/arena.{h,cc} | ultragui::core::arena | ported (no ArenaScope, see quirks) |
| core/color.h | ultragui::core::color | ported |
| core/component_store.{h,cc} | ultragui::core::component_store | ported; ComponentTypeId -> ComponentKey |
| core/config.h, core/types.h | - | gone: aki primitives and akikit containers |
| core/export.h | - | gone: no DLL export macros |
| core/from_chars.cc, from_chars_compat.h | ultragui::core::from_chars | ported |
| core/handle.h | ultragui::core::handle | ported |
| core/math.h | ultragui::core::math | ported |
| core/rect.h | ultragui::core::rect | ported |
| core/string_utils.{h,cc} | ultragui::core::string_utils | ported |
| core/string_view.h | ultragui::core::string_view (+ `str`) | ported |
| style/enums.h | ultragui::style::enums | ported |
| style/property_types.h | ultragui::style::property_types | ported |
| style/style.{h,cc} | ultragui::style::style | ported |
| style/theme.h | ultragui::style::theme | ported |
| style/transition.{h,cc} | ultragui::style::transition | ported |
| animation/animator.{h,cc} | ultragui::animation::animator | ported |
| render/vertex.h | ultragui::render::vertex | ported |
| render/draw_data.h | ultragui::render::draw_data | ported |
| render/texture_backend.h | ultragui::render::texture_backend | ported (seam) |
| render/renderer2d.{h,cc} | ultragui::render::renderer2d | ported (not generic over the Rhi, see "Renderer2D and the Rhi"; vertices bit-exact vs C++) |
| render/paint.{h,cc} | ultragui::render::paint | ported |
| rhi/rhi.h | ultragui::rhi::rhi | ported (capability) |
| rhi/rhi_types.h | ultragui::rhi::rhi_types | ported |
| rhi/rhi_texture_backend.h | ultragui::rhi::rhi_texture_backend | ported |
| rhi/null/null_rhi.cc | ultragui::rhi::null_rhi (`null` is a keyword) | ported |
| rhi/vulkan/* | | pkg:ultragui_vulkan |
| rhi/opengl/* | | pkg:ultragui_opengl |
| rhi/d3d11/* | | pkg:ultragui_d3d11 |
| rhi/d3d12/* | | pkg:ultragui_d3d12 |
| input/input_queue.h | ultragui::input::input_queue | ported |
| input/input.{h,cc} | ultragui::input::input | ported (platform through the WidgetContext, see "InputRouter") |
| platform/platform.h | ultragui::platform::platform | ported (capability + DynPlatform) |
| platform/platform_host.{h,cc} | ultragui::platform::platform_host | ported |
| platform/platform_open_url.cc | ultragui::platform::platform_open_url | ported |
| platform/platform_glfw.cc | | pkg:ultragui_glfw |
| text/text_engine.{h,cc} | ultragui::text::text_engine (+ sys::freetype, sys::harfbuzz) | ported |
| audio/audio_backend.h | ultragui::audio::audio_backend | ported (seam) |
| audio/miniaudio_impl.cc, backends/ugui_impl_miniaudio.* | ultragui_audio::ugui_impl_miniaudio (+ sys::miniaudio, csrc/ugui_ma.c) | ported (pkg:ultragui_audio; `AudioEngine`) |
| scripting/script_runtime.h, script_runtime_none.cc | ultragui::scripting::script_runtime | ported (capability + NoScript) |
| scripting/lua_*.{h,cc} | | pkg:ultragui_lua |
| ultragui_config.h | ultragui::ultragui_config | ported (switch -> package/type map) |
| widgets/widget_registry.{h,cc} | ultragui::widgets::widget_registry | ported (World; no Active()) |
| widgets/components.h | ultragui::widgets::components | ported |
| widgets/widget_context.h | ultragui::widgets::widget_context | ported |
| widgets/widget.h, widget.cc | ultragui::widgets::widget | ported (PaintWidget overloads: paint_widget, paint_widget_styled) |
| widgets/widget_vtable.{h,cc} | ultragui::widgets::widget_vtable | ported (match dispatch, arms added per kind) |
| widgets/widget_tree.{h,cc} | ultragui::widgets::widget_tree | ported |
| widgets/panel.{h,cc} | ultragui::widgets::panel | ported (no vtable arms: the C++ has none) |
| widgets/{text,button,image,checkbox,radio,toggle,slider,dropdown,text_input,scroll_view,context_menu,modal,rich_text,message_box}.{h,cc} | ultragui::widgets::<kind> | next (widgets/scroll_view.aki is a stub with scroll_offset only; the kinds port replaces it) |
| layout/layout.{h,cc}, layout_tree.{h,cc} | ultragui::layout::{layout,layout_tree} (+ sys::yoga) | ported (test_layout_retained.cc ported) |
| idl/parser.{h,cc} | ultragui::idl::parser | ported (AST dumps equal the C++'s on examples/**/*.ugui and a 25-file corpus; AST type names below) |
| idl/builder.{h,cc} | ultragui::idl::builder | next (consumes idl::parser's AST) |
| svg/svg_types.h, svg_parser.cc, svg_rasterizer.cc, svg.{h,cc} | ultragui::svg::{svg_types,svg_parser,svg_rasterizer,svg} (+ sys::c_text) | ported (rasterizer byte-identical to the C++ on a 62-render corpus; 3 endless C++ loops terminate, see quirks) |
| anim/json.{h,cc} | ultragui::anim::json | ported |
| anim/anim_types.h, anim_parser.cc, anim_eval.cc, anim_render.cc | ultragui::anim::{anim_types,anim_parser,anim_eval,anim_render} | ported (rendered frames byte-identical to the C++: 330 frame hashes) |
| anim/vector_animation.{h,cc} | ultragui::anim::vector_animation | ported (backend passed per call, see below) |
| ui_context.{h,cc} | ultragui::ui_context | next |
| lottie/lottie.{h,cc} | ultragui_lottie::lottie (+ sys::rlottie) | ported (pkg:ultragui_lottie; backend passed per call, see "Backends held by value") |
| video/video.{h,cc} | ultragui_video::video (+ sys::pl_mpeg) | ported (pkg:ultragui_video; RHI passed per call; audio bridge included) |
| backends/ugui_impl_vulkan.* | | pkg:ultragui_vulkan |
| backends/ugui_impl_opengl3.* | | pkg:ultragui_opengl |
| backends/ugui_impl_dx11.* | | pkg:ultragui_d3d11 |
| backends/ugui_impl_win32.* | | pkg:ultragui_win32 |
| ultragui.h, pipeline.h | `ultragui` (src/lib.aki) | gone: import modules directly; pipeline.h is prose |

## Naming

- Types PascalCase (`TextEngine`, `WidgetId`); functions, methods and fields
  snake_case (`load_font`, `atlas_revision`); constants `kName`
  (`kInvalidFont`); enum variants without `k` (`FontWeight::Bold`).
- C++ accessor/mutator pairs keep their C++ field-style names
  (`master_volume()`, `set_master_volume()`).
- A field and a method may share a name; where that confuses, the field gets
  a trailing `_` (`atlas_texture_`).
- A constructor is a static function named after the type
  (`TextEngine()`, `World()`); several C++ constructors become named statics
  (`Rect::new`, `Rect::from_pos_size`, `EdgeInsets::all`).
- C++ overloads become distinct names: `LoadFont(path, weight, style)` is
  `load_font_styled`, `LoadFontMemory` is `load_font_memory`,
  `Lerp(Color...)` is `lerp_color`.
- Keywords that cannot be names: `null`, `loop` is fine but `looping` reads
  better for the audio flag. The module for rhi/null is `null_rhi`.
- Every module starts with a comment naming its C++ file(s) and every
  difference in shape (not in behavior) from the C++.

## Idiom map (the concrete types chosen)

| C++ | aki here |
|---|---|
| `Vector<T>` | `List<T>` (akikit::containers::list) |
| `String`, `const char*` text | `String`; parameters `str` |
| `const char* data, usize len` bytes | `bytes` (`&[byte]`); `&[u8]` for pixels the code computes on |
| `HashMap<K, V>` | `HashMap<K, V>` (akikit); iteration order never reaches output, as in the C++ (`sorted_entries`) |
| `Optional<T>`, nullable `T*` result | `T?`, `(&T)?`, `(&var T)?` |
| `bool F(..., T& out)` | `T?` or a tuple result |
| out-params `f32& w, f32& h` | tuple result `(f32, f32)` |
| `Function<R(A)>` member | `own fn(A): R` (e.g. `DragHandler = own fn(Vec2): ()`) |
| `Function` parameter called during the call | `fn(A): R` |
| C callback + `void* user_data` | a function value that captures |
| `UniquePtr<T>` | the value inline, `Box<T>`, or `T?` for "maybe constructed" |
| shared ownership | `Arc<T>` (TextRun shares its glyphs with the shaping cache) |
| `std::sort`/`IntroSort` | `intro_sort` (never akikit's sort: tie order differs) |
| `Min`/`Max`/`ClampMinMax` | `min`/`max`/`clamp_min_max` (+ `_f32` forms) |
| `std::from_chars` | `from_chars_f32`, `from_chars_u32` |
| enum class flags (`WidgetState`, `TextDecoration`) | `open enum` with `with`, `masked`, `without`, `has` |
| abstract interface swapped at link time | `capability` + a generic parameter with a default type |
| abstract interface supplied at run time | erased seam: capability + `Dyn<X><'a>` + `<X>Box` |
| `static` counters (NextWidgetId, WidgetRevision) | `static let` `Atomic<T>` |
| thread-local "active registry" | gone: pass the World |
| `assert(x)` the C++ relies on | `@stop` / `!` on the same condition |
| `static_cast<int>(float)` | `@saturating_cast` (what aarch64 does; UB in C++) |
| printf/fprintf logging | `println` / `eprintln` from akikit::logging, same text |

## Seams

Two kinds. **Link-time** C++ classes (one concrete class whose `.cc` is
swapped) are capabilities that UIContext is generic over, with a default
type. **Run-time** host-supplied virtual interfaces are erased seams: the
capability, a borrowed `Dyn<X><'a>` that core code takes where the C++ took
`X*`, and an owned `<X>Box` for an owner that keeps one. `core::erased`
(`ErasedRef<'a>`, `ErasedBox`) carries the object; each method is a
`static fn` thunk instantiated for the concrete type. The unsafe code lives
in `core::erased` and the thunks; the public API is safe. Copy
`render/texture_backend.aki` or `audio/audio_backend.aki` for a new one.

UIContext (next wave) should be:

```aki
pub struct UIContext<P: Platform = HostPlatform, R: Rhi = NullRhi, S: ScriptRuntime = NoScript> { ... }
```

### Rhi (link-time) — `ultragui::rhi::rhi`

```aki
pub capability Rhi {
  fn init(var self&, config&: RhiConfig): bool;
  fn shutdown(var self&);
  fn begin_frame(var self&, clear_color: Color): bool;
  fn end_frame(var self&);
  fn set_scissor(var self&, rect: Rect);
  fn reset_scissor(var self&);
  fn draw_triangles(var self&, vertices&: [Vertex2D], indices&: [u32], texture: RhiTextureHandle);
  fn draw_text_triangles(var self&, vertices&: [Vertex2D], indices&: [u32], atlas_texture: RhiTextureHandle);
  fn create_texture(var self&, width: u32, height: u32, format: RhiFormat, pixels: bytes, filter: RhiFilter): RhiTextureHandle;
  fn update_texture(var self&, handle: RhiTextureHandle, pixels: bytes);
  fn destroy_texture(var self&, handle: RhiTextureHandle);
  fn acquire_frame(var self&): bool;
  fn create_render_target(var self&, width: u32, height: u32): RhiTextureHandle;
  fn destroy_render_target(var self&, handle: RhiTextureHandle);
  fn begin_offscreen(var self&, target: RhiTextureHandle, clear_color: Color): bool;
  fn end_offscreen(var self&, target: RhiTextureHandle);
  fn convert_video_frame(var self&, target: RhiTextureHandle, y: RhiTextureHandle, cb: RhiTextureHandle, cr: RhiTextureHandle);
  fn display_size(self&): Vec2;
  fn dpi_scale(self&): f32;
}
```

`NullRhi` (rhi::null_rhi) is the default. `RhiConfig::native_window` is filled
from `Platform::native_handle()`; an RHI never sees the Platform type. Empty
`bytes` is the C++ `nullptr` pixels. A backend package implements it:
`pub struct VulkanRhi : Rhi { ... }`.

### Platform (link-time) — `ultragui::platform::platform`

```aki
pub capability Platform {
  fn init(var self&, config&: WindowConfig): bool;
  fn shutdown(var self&);
  fn should_close(self&): bool;
  fn poll_events(var self&);
  fn window_size(self&): Vec2;
  fn framebuffer_size(self&): Vec2;
  fn dpi_scale(self&): f32;
  fn time(self&): f64;
  fn native_handle(self&): *var u8;
  fn set_cursor(var self&, cursor: Cursor);
  fn clipboard_text(var self&): str;
  fn set_clipboard_text(var self&, text: str);
  fn open_url(var self&, url: str) { open_url(url); }   // default body: ULTRAGUI_DEFAULT_OPEN_URL
  fn input_queue(var self&): &var InputQueue;
}
```

`HostPlatform` (platform::platform_host) is the default; `host.feed()` gives a
`Send` `HostFeed` for another thread (the C++ `host::` functions). Code that
must reach "the platform" without being generic over it (widgets, the input
router) takes `DynPlatform<'a>`:

```aki
fn mouse_x(var p&: DynPlatform): f32 {
  p.poll_events();
  return p.input_queue().mouse_pos.x;
}
var host = HostPlatform();
var p = DynPlatform::of(host);
let x = mouse_x(p);
```

### ScriptRuntime (link-time) — `ultragui::scripting::script_runtime`

```aki
pub capability ScriptRuntime {
  fn init(var self&): bool;
  fn shutdown(var self&);
  fn exec(var self&, var world&: World, var ctx&: WidgetContext, script: str, name: str): bool;  // name: kDefaultChunkName
  fn exec_file(var self&, var world&: World, var ctx&: WidgetContext, path: str): bool;
  fn register_widget(var self&, world&: World, widget: WidgetId);
  fn unregister_widget(var self&, world&: World, widget: WidgetId);
  fn clear_widget_registry(var self&);
  fn call_handler(var self&, var world&: World, var ctx&: WidgetContext, func_name: str, widget: WidgetId): bool;
  fn find_registered_widget(self&, name: str): WidgetId;
  fn sync_timer_clock(var self&, current_time: f64);
  fn clear_timers_and_tweens(var self&);
  fn update_timers(var self&, var world&: World, var ctx&: WidgetContext, current_time: f64);
  fn wire_change_handlers(var self&, var world&: World, root: WidgetId);
}
```

`NoScript` is script_runtime_none.cc and the default. Methods that can touch
a widget take the World (and the context) because the C++ reached them via
`WidgetRegistry::Active()`. Lua-only members (ScheduleTimer, RegisterFunction,
state()) live on ultragui_lua's type, which keeps the World pointer only for
the duration of a call into Lua.

### TextureBackend (run-time) — `ultragui::render::texture_backend`

```aki
pub capability TextureBackend {
  fn create_texture(var self&, width: u32, height: u32, format: RhiFormat, pixels: bytes, filter: RhiFilter): TextureId;
  fn update_texture(var self&, id: TextureId, pixels: bytes);
  fn destroy_texture(var self&, id: TextureId);
}
pub struct DynTextureBackend<'a> : TextureBackend { pub fn of<'a, T: TextureBackend>(backend: &'a var T): DynTextureBackend<'a>; ... }
pub struct TextureBackendBox : TextureBackend { pub fn of<T: TextureBackend>(backend: T): TextureBackendBox; pub fn as_dyn(var self&): DynTextureBackend; ... }
pub fn texture_id_from_rhi_handle(h: RhiTextureHandle): TextureId;   // h + 1, kInvalidTexture -> kNullTextureId
pub fn rhi_handle_from_texture_id(id: TextureId): RhiTextureHandle;
```

`RhiTextureBackend<'a, R: Rhi>` (rhi::rhi_texture_backend) adapts an Rhi in
legacy mode. Usage:

```aki
fn upload(var tex&: DynTextureBackend): TextureId => tex.create_texture(1, 1, RhiFormat::Rgba8Unorm, pixels, RhiFilter::Linear);
var boxed = TextureBackendBox::of(MyHostTextures {});   // UIContext::set_texture_backend
var lent = boxed.as_dyn();                               // bind it: a temporary is not a `var` place
let id = upload(lent);
```

### AudioBackend (run-time) — `ultragui::audio::audio_backend`

```aki
pub type SoundHandle = u32;  pub comptime let kInvalidSound: SoundHandle = 0;
pub capability AudioBackend {
  fn init(var self&): bool;              fn shutdown(var self&);          fn is_initialized(self&): bool;
  fn play(var self&, path: str, volume: f32, looping: bool): SoundHandle;
  fn load(var self&, path: str): SoundHandle;
  fn play_loaded(var self&, preloaded: SoundHandle, volume: f32, looping: bool): SoundHandle;
  fn stop(var self&, handle: SoundHandle);   fn is_playing(self&, handle: SoundHandle): bool;
  fn set_volume(var self&, handle: SoundHandle, volume: f32);   fn set_pan(...);   fn set_pitch(...);
  fn seek(var self&, handle: SoundHandle, seconds: f32);
  fn set_master_volume(var self&, volume: f32);   fn master_volume(self&): f32;
  fn stop_all(var self&);   fn pause_all(var self&);   fn resume_all(var self&);
}
```

`DynAudioBackend<'a>`, `AudioBackendBox` (`AudioBackendBox::silent()` holds a
`NullAudioBackend`, UIContext's default), as for textures. The C++ defaults
(volume 1, loop false) are written out at call sites.

### TextEngine — `ultragui::text::text_engine`

Not a seam, but everything draws through it:

```aki
var te = TextEngine();
te.init();                                   // Init(RHI*): the RHI part is below
te.set_dpi_scale(rhi.dpi_scale());           // the C++ reads rhi_->dpi_scale() in Shape; 1.0 with no RHI
let f = te.load_font("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
te.begin_frame();
let run = te.shape(f, "Hello", 16.0);        // TextRun: glyphs() -> &[Glyph], total_advance, ascent, ...
let lay = te.layout(run, TextLayoutConfig { max_width: 120.0 });   // owns its lines
te.flush_atlas(rhi);                         // generic over R: Rhi; creates then updates the R8 atlas
te.shutdown_with(rhi);                       // or shutdown() in draw-data mode
```

A `TextRun` shares its glyphs with the shaping cache (`Arc`), so it stays
valid as long as it is held, past `begin_frame` (the C++ only promises until
then). `TextRun()` is the empty run. Renderer2D's DrawTextLayout should draw
`run.glyphs()[line.glyph_start..line.glyph_start + line.glyph_count]`.

### World and components — `ultragui::widgets::widget_registry`, `core::component_store`

Component types are open. Each has a `ComponentKey<C>`, a `static let` beside
the type; the key's dense id (assigned process-wide on first use, as
ComponentTypeId is) indexes the World's stores, which are created on first
use and held erased with a remove thunk:

```aki
pub struct ButtonContent { ... }
pub static let kButtonContent: ComponentKey<ButtonContent> = ComponentKey();

var world = World();
let e = world.create();                       // New(): WidgetNode, Transform, StyleC, Hierarchy attached
world.add(kButtonContent, e, ButtonContent { ... });      // returns &var C; ignore it as a statement
if (let c& = world.get(kButtonContent, e)) { ... }        // (&C)?, none if absent or stale
if (var c& = world.get_var(kButtonContent, e)) { ... }    // (&var C)?
var s& = world.get_or_add(kStateStyle, e, StateStyle {}); // GetOrAdd: the fallback is the C++ C{}
let (t, h) = world.get_pair_var(kTransform, kHierarchy, e);  // two components at once
world.has(kTooltip, e); world.remove(kTooltip, e);
for (let id in world.store(kButtonContent)!.ids()) { ... }   // dense iteration: ids() / as_slice()
world.release(e);                              // drops every component; handles go stale
```

Host components work the same way, from any package; a component's drop is
its release notification. Keys: `kWidgetNode`, `kTransform`, `kHierarchy`,
`kStyleC` (widgets::widget); `kTooltip`, `kMovable`, `kDragHandle`,
`kStateStyle`, `kAnimStyle` (widgets::components). Each widget kind module
declares its own (`kTextContent`, `kButtonContent`, ...).

A view from `get`/`get_var`/`add` borrows the whole World (the C++ "valid
until the next structural change", enforced). Copy out what you need
(`Transform`, `StyleC`, `Style` are `Copy`; `to_list(h.children.as_slice())`
for a child list you walk while mutating), then call the next function.

There is no `WidgetRegistry::Active()`: every function that touches entities
takes `var world&: World` first. `WidgetRegistry` remains as an alias of
`World`.

### WidgetContext — `ultragui::widgets::widget_context`

```aki
pub struct WidgetContext<'a> {
  pub var text_engine: (&'a var TextEngine)? = none
  pub var default_font: FontHandle = kInvalidFont
  pub var animator: (&'a var Animator)? = none
  pub var current_time: f64? = none
  pub var platform: DynPlatform<'a>? = none
  pub var ui_scale: f32 = 1.0
  pub fn detached(): WidgetContext<'static>;
}
```

The context is an argument, built per call from views of the owner's
subsystems while the World is lent separately (field-disjoint borrows work):

```aki
struct Ui { pub var world: World  pub var text: TextEngine  pub var animator: Animator  pub var last_time: f64 ... }
fn Ui::set_state(var self&, e: WidgetId, s: WidgetState) {
  var ctx = WidgetContext { text_engine: self.text, animator: self.animator, current_time: self.last_time };
  set_widget_state(self.world, ctx, e, s);
}
```

A widget entry reaches a subsystem through the context, reading plain fields
before it takes a subsystem's write view (that view borrows the context until
its last use); tests/world_test.aki's `label_width` is the model:

```aki
fn label_width(var world&: World, var ctx&: WidgetContext, e: WidgetId, label: str): f32 {
  let c& = widget_context_of(world, ctx, e) else { return 0.0; };   // C++: ctx == nullptr
  let font = c.default_font;
  let sc = ui_scale(world, c, e);
  var te& = c.text_engine else { return 0.0; };                    // C++: !ctx->text_engine
  return te.shape(font, label, 16.0 * sc).total_advance;
}
```

`WidgetNode::has_context` is the C++ non-null `context` pointer:
`set_context(world, e, true)` attaches a subtree, `add_child` propagates it,
and widget code asks `widget_context_of(world, ctx, e)` (none where the C++
pointer is null) or `has_widget_context(world, e)`. `ui_scale(world, ctx, e)`
is 1.0 for a detached widget, as in the C++.

### Renderer2D and the Rhi — `ultragui::render::renderer2d`

The API is the C++ one in snake_case (`draw_rect`, `draw_rect_gradient`,
`draw_shadow`, `draw_inset_shadow`, `draw_bordered_rect`, `draw_text`,
`draw_text_layout`, `draw_radial_gradient`, `draw_multi_stop_gradient`,
`draw_textured_rect`, `push_scissor`/`pop_scissor`,
`push_transform`/`pop_transform`, `set_next_blur`, `set_display_size`,
`get_draw_data`), same parameter order, C++ defaults as aki defaults. The one
parameter-shape exception: `GradientStop* stops, u32 count` is
`stops: &[GradientStop]` (`s.gradient_stops[..s.gradient_stop_count as usize]`).
Widget code takes `var renderer&: Renderer2D` (no lifetime, no type parameter).

Renderer2D is not generic over the Rhi and keeps no view of it. The C++
reads `rhi_->display_size()` and `rhi_->dpi_scale()` while drawing; here an
attached renderer keeps a snapshot of both, taken by `init_rhi` and refreshed
by `begin_frame_rhi` (nothing changes them between begin and end of a frame):

```aki
var r = Renderer2D();
r.init();                         // Init(nullptr): draw-data mode
r.set_display_size(viewport);
r.set_texture_backend(TextureBackendBox::of(host_textures));   // owned; none = flat gradients
r.begin_frame();  paint...;  let dd = r.get_draw_data();       // DrawData views r's buffers

r.init_rhi(rhi);                  // Init(&rhi): legacy mode
r.set_rhi_texture_backend();      // gradients through the Rhi (UIContext's RHITextureBackend)
r.begin_frame_rhi(rhi);  paint...;  r.end_frame(rhi);           // submits in emission order
```

Gradient textures: with a host backend (owned `TextureBackendBox`) they are
created at draw time, as in the C++. The legacy RHI adapter borrows the Rhi
its owner keeps, so the renderer cannot hold it; in Rhi-texture mode a new
gradient gets a provisional id (counting down from `kFontTextureId - 1`,
above anything an Rhi mints) and `end_frame(rhi)` (or
`resolve_textures(rhi)`, for a get_draw_data user) creates the pending
textures in request order and patches batches and cache before submitting.
Batching only compares ids, so the batches are the C++ ones
(renderer2d_test checks both modes against the C++). `texture_backend()`
lends the owned host backend to other subsystems (UIContext's
`texture_backend()` in draw-data mode).

### Layout — `ultragui::layout::{layout,layout_tree}`

```aki
var engine = LayoutEngine();
compute_widget_layout(world, ctx, root, LayoutViewport { width: w, height: h, scale: s }, engine);  // ComputeWidgetLayout
engine.store_for(root_id)          // StoreFor -> &var NodeStore
engine.compute(nodes, vp);         // Compute over the caller's own &var [LayoutNode]
engine.compute_store(root_id, vp); // Compute over store_for(root_id).nodes (borrowing the engine once)
engine.reset();
```

`kNoNode` is the C++ `~0u`. Yoga's C API is bound by hand in `sys::yoga`;
the measure callback is an `extern fn` reading the node's context, which
compute re-points at the caller's LayoutNode on every call.

### Paint and the widget tree

```aki
paint_widget_tree(world, ctx, root, renderer);        // render::paint, PaintWidgetTree
paint_widget(world, ctx, e, renderer);                // PaintWidget(world, e, r)
paint_widget_styled(world, ctx, e, renderer, style);  // PaintWidget(world, e, r, computed)
measure_widget_tree(world, ctx, root): u32;  update_widget_tree(world, ctx, root, dt);
find_widget(world, root, name);  find_widget_by_id(world, root, id);
input_to_layout_point(world, e, point);  populate_layout_node(world, e, node);  apply_layout_result(world, e, node);
create_panel(world, id);
```

The tree walks copy a child list before recursing into it (the C++ walks the
live vector; nothing in a walk restructures the tree).

### InputRouter — `ultragui::input::input`

The router reaches the platform through `ctx.platform` (no `Init(Platform*)`;
UIContext points both at one platform, and `ctx.platform == none` is the C++
`!platform_`). Everything that touches widgets takes the World and the
context:

```aki
var router = InputRouter();
router.set_on_click(own fn(w: WidgetId, b: MouseButton) { ... });   // ClickHandler, HoverHandler,
router.register_shortcut(key, mods, own fn() { ... });              // ShortcutHandler, GamepadBackHandler
let consumed = router.process(world, ctx, root);
router.set_focus(world, ctx, e);  router.set_hover(world, ctx, e);
router.refresh_hover(world, ctx, root);  router.reset_state(world, ctx);
```

process copies the platform's queue (widget calls need the context, which
lends the platform) and clears the platform's queue at the end, as the C++
does. Handlers keep the C++ signatures and get no World: an application
handler that must reach widgets or the script runtime (the examples' Lua
`on_<name>` dispatch) records the widget and acts after process returns.

`widgets::scroll_view` exists as a stub (ScrollViewContent,
kScrollViewContent, scroll_offset) in a commit titled "stub scroll_view ...":
the widget-kinds port replaces the file.

### Widget dispatch — `ultragui::widgets::widget_vtable`

Per-kind behaviour is a `match` on `WidgetKind` per vtable entry, calling the
kind's functions directly:

```aki
pub fn vt_measure(kind: WidgetKind, var world&: World, var ctx&: WidgetContext, e: WidgetId): (f32, f32)? => match (kind) {
  WidgetKind::Button => button_measure(world, ctx, e),     // the arm a kind's port adds
  _ => none,                                               // the C++ null entry
};
```

Entries: `custom_paint`, `vt_draw` (bool: had an entry), `vt_measure`,
`vt_hit_test`, `vt_on_click`, `vt_on_update`, `vt_on_scroll`, `vt_on_layout`,
`vt_on_key_down`, `vt_on_char_input`, `vt_consumes_text_input`,
`vt_on_dismiss`. A kind function has the shape
`fn button_measure(var world&: World, var ctx&: WidgetContext, e: WidgetId): (f32, f32)`
(the C++ out-params as results). The free functions in widgets::widget
(`measure_widget`, `click_widget`, `hit_test`, `layout_widget`, ...) apply the
C++ fallbacks. A widget kind port: its module (content struct + key +
`create_<kind>` + entry functions), its arms here, its test.

Callbacks stored in components (`on_click`, `on_change`) are `own fn` values.
One that needs the World gets it as a parameter, not a capture; the caller
takes the callback out of the component (or copies what it needs) before
calling it, because the component's view borrows the World.

### Subsystems that held a backend pointer: LottieAnimation, VideoPlayer

The C++ `LottieAnimation` keeps the `TextureBackend*` it was loaded with and
`VideoPlayer` keeps the `RHI*`. A struct cannot hold a borrow of something its
own owner (UIContext) also owns, so the ported types hold neither: every
method that can render, upload or free takes the backend as an argument, the
way `TextEngine::flush_atlas(rhi)` does.

```aki
var anim = LottieAnimation();
var tex = DynTextureBackend::of(host_textures);       // or boxed.as_dyn(); null C++ pointer = `none`
anim.load(tex, "spinner.json", 64, 64);               // backend: (&var DynTextureBackend)?
anim.update(dt, tex);  anim.seek(0.5, tex);  anim.stop(tex);
anim.unload(tex);                                     // releases the texture; do this before drop

var vid = VideoPlayer();
vid.load(rhi, "cutscene.mpg", none);                  // rhi: var R (R: Rhi); audio: (&DynAudioBackend)?
vid.update(dt, rhi);  vid.convert_frame(rhi);  vid.seek(t, rhi);  vid.stop(rhi);
vid.unload(rhi);                                      // destroys Y/Cb/Cr and the render target
```

Pass the same backend/RHI the animation was loaded with. A drop without
`unload` frees the CPU side (rlottie animation, pl_mpeg decoder, audio device)
and leaves the GPU handles to the backend's own teardown. Methods that touch
neither (`play`, `pause`, `set_speed`, `is_playing`, `progress`, ...) take no
backend. UIContext's `LoadLottie`/`LoadVideo` store the handles' players in
maps and pass `texture_backend.as_dyn()` / the Rhi on each `update`.

### Optional packages and their native code

* `ultragui_audio`: miniaudio is `[build.implement]` from `third_party`
  (`MINIAUDIO_IMPLEMENTATION`), plus `csrc/ugui_ma.c`, a ~80-line shim that
  reports the sizes of miniaudio's opaque structs (aki allocates them as
  zeroed `NativeBlock`s), fills the config structs miniaudio takes by value,
  and wraps the playback device's callback. Everything else is declared by
  hand in `sys::miniaudio`. `AudioEngine` is an `AudioBackend`; wrap it in
  `AudioBackendBox::of(AudioEngine())` for `UIContext::set_audio`.
* `ultragui_lottie`: rlottie's sources compile through `c_sources` (the file
  list of its CMake, config.h in `csrc/`), via its C API
  (`lottie_animation_*`). No flake change; it builds anywhere akiko has a C++
  compiler (Windows too: `-DRLOTTIE_BUILD=0`, `-lshlwapi`; not built here, the
  `--target x86_64-w64-windows-gnu` check only covers aki code). `opt = 3` to
  match the reference build's -O3 (rlottie's rasterizer is slow below it).
* `ultragui_video`: pl_mpeg is `[build.implement]` with
  `-includestddef.h -includestdio.h` (the header expects its includer to have
  them); it depends on `ultragui_audio` for the miniaudio binding and
  implementation (the audio bridge uses `ma_pcm_rb` and a playback device).

## Fidelity

Reproduce the C++ exactly: float expressions in C++ operation order, the
same rounding, IntroSort (never akikit's sort), from_chars, the same log text
on the same stream. Where C++ behavior is undefined, do what aarch64 does and
say so (`@saturating_cast` for float->int). akiko fuses `a * b + c` into an
fma in every build (language ADR 0221), as clang does for C++ on aarch64
(-ffp-contract=on); where the C++ splits a product and a sum across
statements, keep the product in a local so it rounds alone
(style/transition.aki, cubic_bezier). On x86-64 the C++ build does not fuse,
so bit-exact tables are aarch64's.

### Known C++ quirks kept

- `ParseHexColor` succeeds with what it read when a digit run stops early
  ("#12G456" -> 0x12): only the error code is checked.
- `ArenaScope` resets the whole arena, not to its saved offset; nothing uses
  it, so it is not ported rather than ported wrong.
- `Clamp` (core::math) returns `lo` for `v < lo` even when `lo > hi`;
  `clamp_min_max` is std::clamp. Both exist; use the one the C++ calls.
- `Style::Lerp` leaves align_content, text_transform, text_decoration, cursor
  and pointer_events at `a`'s values.
- Pack functions spill a channel above 1.0 into the next byte, as the C++
  shifts do.
- The shaping cache key compares floats with `==` but hashes their bits
  (`-0.0` and `0.0` may cache twice), as the C++.
- TextEngine's bitmap copy assumes FreeType's positive pitch (the C++ computes
  `row * pitch` unsigned).
- `.ugui` lexer: a byte that starts no token is a one-byte error token in the
  C++; a multi-byte UTF-8 character here is one error token holding the whole
  character (a String cannot hold half of one). A file that is not UTF-8 is
  the error "file is not valid UTF-8" (the C++ parses any bytes); a .uganim
  that is not UTF-8 is a JSON parse error.
- `.ugui` import resolution keeps the C++'s `std::filesystem` imitations
  (ParentPath, JoinPath, LexicallyNormal, WeaklyCanonical) with akikit's
  `real_path`/`path_exists`; Windows uses the same prefix algorithm (the C++
  calls `_fullpath` there).
- NOT kept, on purpose: the C++ `render_anim_frame` caches parsed path layers
  in a thread_local map keyed by the *address* of `AnimLayer::path_data`, so
  a document allocated at a freed document's address renders the old paths.
  The port parses a path layer each frame (what the cache returns when it is
  right); the reference tool leaks its documents so the C++ never hits it.
- VectorAnimation of a looping document with duration 0 spins forever in
  `update` (`while (t >= dur) t -= dur`), as the C++.
- JSON numbers go through C `strtod`, so "-inf", "-nan(12)" and hex floats
  parse as numbers, and `\u0041` reads as "u0041" (no `\u` decoding).
- KeyframeAnimation with a zero duration: the C++ casts inf to int (UB); the
  port saturates.
- SVG: a gradient `<stop>` without `offset` has an uninitialized offset in the
  C++ (the port uses 0); `style=` on a `<stop>` is parsed and dropped.
- SVG: three C++ loops never end on malformed input (text or a stray close
  tag before the root; path data or polyline `points` that stop being
  numbers, which would also grow the path without bound). The port stops at
  the first iteration that consumes no input; every terminating input behaves
  as the C++.
- SVG: attribute values are C strings, so a value is cut at its first NUL and
  numbers are libc's `strtof` (viewBox, width/height, path data) or
  `atof`-through-`strtod` rounded to f32 (everything else), as in the C++;
  `sys::c_text` calls libc for both so the values cannot differ.
- `VideoPlayer::Load(rhi, path, AudioBackend* audio)` only tests `audio` for
  non-null; the audio plays through the bridge's own `ma_device`, not through
  the backend. The port takes `(&DynAudioBackend)?` and tests it for `some`.
- pl_mpeg's video callback uploads every decoded frame to the RHI inside
  `plm_decode`/`plm_seek`. The port copies each frame's planes in the
  callback and uploads them in order when the call returns (no RHI in
  reach of a C callback): same RHI calls, same contents, uploaded at the end
  of the call.
- `LottieAnimation` computes `frameAtPos` itself (rlottie's formula in
  `double`): the C API's `lottie_animation_get_frame_at_pos` narrows the
  position to `float` and can pick a different frame than the C++ call.
- The audio bridge's `volume`/`muted` are plain fields read by the audio
  thread while the main thread writes them, as in the C++.
- Renderer2D::DrawTextLayout computes a slack against `max_width` and throws
  it away: every line is drawn left-aligned, whatever max_width says.
- Renderer2D's gradient cache keys collide as the C++ ones do (the
  multi-stop hash XORs shifted fields; a collision reuses the texture).

### Shape differences (not behavior)

- **VectorAnimation does not store its TextureBackend.** `load(backend, path,
  w, h)`, `load_data(backend, json, w, h)`, `update(backend, dt)` and
  `unload(backend)` take it as `(&var T)?` for any `T: TextureBackend` (a
  `DynTextureBackend`, a `TextureBackendBox`, an `RhiTextureBackend`; `none`
  is the C++ `nullptr`), because a stored borrow would pin UIContext, which
  owns both. The destructor therefore cannot destroy the texture: call
  `unload(backend)` first. Pass the same backend each time.
- **idl::parser AST**: `UguiNode::type` is `type_`; the nested C++ types are
  module-level: `UguiStateBlock`, `UguiKeyframeBlock` (+ `UguiKeyframeStop`),
  `UguiMediaQuery`, `UguiStyleClass`, `UguiComponent`; `UguiDocument` and
  `ParseError` as in the C++. Entry points: `parse_ugui(source: str,
  filename: str, doc, errors): bool` and `parse_ugui_file(path, doc, errors)`;
  every AST type is `Clone`. Property maps are `HashMap<String, String>`
  (walk them with `sorted_entries`, as the C++ builder does).
- **anim**: `AnimDocument::loop` is `looping`; `parse_anim_data(json: str,
  doc)` and `parse_anim_file(path, doc)` take text (a non-UTF-8 file is a
  parse error); `json::JsonValue::type` is `kind`; `evaluate_layer` returns
  the `EvaluatedLayer`; `render_anim_frame(doc, time, pixels: &var [u8], w,
  h)` builds its svg Document per call and parses path layers each frame
  (the C++ caches by the address of `path_data`).

- SVG: `LoadSvg*` return `SvgImage?`, `ParseSvg` returns `Document?`,
  `Rasterize` writes into a `&var [u8]`, `LoadSvgTexture` takes `(&var T)?`
  for any `TextureBackend` (a `DynTextureBackend` is one); the rasterizer's
  `thread_local` scratch vectors are locals. `w * h * 4` is a `usize`.

- No thread-local active registry; World passed explicitly.
- WidgetContext passed, not pointed to; `current_time` is a value.
- TextEngine takes the dpi scale and the Rhi as arguments; runs share glyphs.
- SetWidgetVTable (no users) and run-time vtable install are gone.
- Calls into a TextEngine before `init()` are no-ops where the C++ crashes.

## Pitfalls and compiler workarounds (akiko 0.7.0)

- **`static fn` types cannot name lifetimes**, so a call through a
  `static fn(&var World, &var WidgetContext, ...)` value fails ("cannot store
  a view of 'world' through 'ctx'"). Dispatch with `match` and direct calls
  (widget_vtable) or keep only one lifetime-carrying view per signature.
- **No function-local statics**, and **static capability methods cannot be
  called through a type parameter** (`T::slot()` is "unknown module T"). Hence
  ComponentKey values instead of a per-type id.
- **`let _ = f()` where `f` returns a view** is "cannot move out of the view":
  write the call as a statement (`world.add(k, e, v);`).
- **A temporary is not a `var` place**: `f(boxed.as_dyn())` for a
  `var x&: DynX` parameter fails; bind it (`var d = boxed.as_dyn(); f(d);`).
- **A closure cannot capture a field of `s` while another field of `s` is
  lent to the call**: bind views of both fields first
  (`var w& = ui.world; var a& = ui.animator; a.update(t, fn(...) { ... w ... });`).
- **`push_c8` stops on a byte >= 0x80** (a String is UTF-8): to build text from
  source bytes, copy runs between ASCII delimiters with `push_str(s[a..b])`.
- **A ternary in an f-string hole needs parentheses and typed literals**:
  `{(ok ? 1i32 : 0i32)}`.
- **`if (var x& = list.at_var(i))` on a `List<T?>`** fails ("needs a place");
  use the index place `list[i]`.
- **`&raw` takes a place, not a parameter's target**: bind the view first
  (`var target& = object; &raw var target`), see core::erased.
- **Struct literal of a type with private fields** works only in that type's
  own methods: give it a private `Type::from_parts(...)` and call that.
- `[u8; N] as bytes` does not cast; slice first (`arr[..] as bytes`). `_` is
  not a type placeholder in casts.
- A view of a slice is already a view: `let out = list.as_slice_var()[a..b];`
  not `var out& = ...`.
- Type arguments of generic calls with only literal arguments need a suffix
  (`min(3i32, 2)`).
- `null` is a keyword (no method named `null`).
- **`for (var b in list.iter_var())` binds a copy**: writes to `b` are lost.
  Write through a view (`for (var b& in list)`) or the index (`list[j].f = x`).
- Integer `+ - *` stop on overflow; hashes need the wrapping forms
  (`h = h *% 1099511628211`). Shifts drop shifted-out bits unchecked.
- C `long` differs per OS: use `CLong`/`CULong` from `ultragui::sys::c_long`
  (one module per OS under `os/`; each OS table in aki.toml needs its own
  directory, they cannot share one). Check Windows layouts with
  `akiko check --target x86_64-w64-windows-gnu`.
- akiko adds no runpath for C libraries: the dev shell's `LD_LIBRARY_PATH`
  carries libstdc++, FreeType and HarfBuzz; a new C dependency needs its lib
  directory there too (flake.nix shellHook).
- **Float fusion is per expression, and matches clang's on aarch64** for the
  SVG rasterizer (`a*b + c*d`, `a*b - c*d - e*f`, `x*y + z*w + e`: 19 of the
  62 corpus renders change when the C++ is built with `-ffp-contract=off`, and
  all 62 match the default build). Vec2/Transform operator methods never
  fuse, as the C++ operators do not: write `p + n.scale(w)` where the C++
  writes `p + n * w`.
- `match` patterns take integer literals, not `'x' as u8`: match on the code
  (`109 => { // 'm'`).
- `fn` literals as `less`/`erase_if` predicates may capture locals
  (`fn(a&: usize, b&: usize): bool => edges[a].y0 < edges[b].y0`).
- yoga's sources warn about `_FORTIFY_SOURCE` without `-O`; `-Wno-#warnings`
  in `c_flags` silences it.
- **Private fields of a module-private struct are private to its methods**:
  helper structs used by free functions in the same file (`SoundSlot`, `Impl`
  in ugui_impl_miniaudio) need `pub var` fields.
- **`unowned` goes after `pub`**: `pub unowned var device: *var T`.
- **Multi-line text is `r"""` ... `"""`** (raw, closer on its own line); a
  plain `"` cannot span lines. Handy for JSON in tests.
- **`[build.implement]` flags**: one argument per flag, and an argument not
  starting with `-` is a macro: `"-includestdio.h"`, not `"-include", "stdio.h"`.
- **`@slice(p, n)` / `@slice_var` results are already views**: `let s = ...`,
  not `let s& = ...`.
- **A struct can hold an `extern fn` value** (`pull: extern fn(*var u8, *var f32, u32): ()`
  in an extern signature), made with `@c_fn(callback_name)`; the callback's user
  pointer to a heap object comes from `Box::as_ptr` (stable while the Box lives).
- Run tests from the package dir; data files are found with `@package_dir()`
  (`f"{@package_dir()}/../../examples/demo.mpg"`).
