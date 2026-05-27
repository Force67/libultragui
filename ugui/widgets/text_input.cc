#include <ugui/platform/platform.h>
#include <ugui/render/renderer2d.h>
#include <ugui/render/vertex.h>
#include <ugui/text/text_engine.h>
#include <ugui/widgets/text_input.h>
#include <ugui/widgets/widget_registry.h>

#include <algorithm>
#include <cstring>

// GLFW key codes (avoid including GLFW in widget code).
namespace {
constexpr int kKeyEnter = 257;
constexpr int kKeyKpEnter = 335;
constexpr int kKeyEscape = 256;
constexpr int kKeyBackspace = 259;
constexpr int kKeyDelete = 261;
constexpr int kKeyRight = 262;
constexpr int kKeyLeft = 263;
constexpr int kKeyDown = 264;
constexpr int kKeyUp = 265;
constexpr int kKeyHome = 268;
constexpr int kKeyEnd = 269;
constexpr int kKeyA = 65;
constexpr int kKeyC = 67;
constexpr int kKeyV = 86;
constexpr int kKeyX = 88;
constexpr int kModShift = 0x0001;
constexpr int kModControl = 0x0002;
}  // namespace

namespace ugui {
namespace {

TextEngine* text_engine(WidgetRegistry& world, wid e) {
  const WidgetContext* ctx = WidgetContextOf(world, e);
  return ctx ? ctx->text_engine : nullptr;
}

FontHandle effective_font(WidgetRegistry& world, wid e,
                          const TextInputContent& c) {
  if (c.font != kInvalidFont) return c.font;
  const WidgetContext* ctx = WidgetContextOf(world, e);
  return ctx ? ctx->default_font : kInvalidFont;
}

// --- UTF-8 navigation -------------------------------------------------------

u32 NextPos(const String& text, u32 pos) {
  if (pos >= text.size()) return static_cast<u32>(text.size());
  u8 c = static_cast<u8>(text[pos]);
  if (c < 0x80) return pos + 1;
  if ((c & 0xE0) == 0xC0) return std::min(pos + 2, static_cast<u32>(text.size()));
  if ((c & 0xF0) == 0xE0) return std::min(pos + 3, static_cast<u32>(text.size()));
  return std::min(pos + 4, static_cast<u32>(text.size()));
}

u32 PrevPos(const String& text, u32 pos) {
  if (pos == 0) return 0;
  --pos;
  while (pos > 0 && (static_cast<u8>(text[pos]) & 0xC0) == 0x80) --pos;
  return pos;
}

// --- Cursor <-> glyph mapping ----------------------------------------------

f32 CursorXFromPos(const String& text, u32 byte_pos, const TextRun& run) {
  f32 x = 0.0f;
  u32 src_pos = 0;
  for (u32 i = 0; i < run.glyph_count && src_pos < byte_pos; ++i) {
    x += run.glyphs[i].x_advance;
    src_pos = NextPos(text, src_pos);
  }
  return x;
}

u32 PosFromX(const String& text, f32 local_x, const TextRun& run) {
  f32 x = 0.0f;
  u32 src_pos = 0;
  for (u32 i = 0; i < run.glyph_count; ++i) {
    f32 mid = x + run.glyphs[i].x_advance * 0.5f;
    if (local_x < mid) return src_pos;
    x += run.glyphs[i].x_advance;
    src_pos = NextPos(text, src_pos);
  }
  return static_cast<u32>(text.size());
}

// --- Selection helpers ------------------------------------------------------

void DeleteSelection(TextInputContent& c) {
  if (c.sel_start == c.sel_end) return;
  u32 lo = std::min(c.sel_start, c.sel_end);
  u32 hi = std::max(c.sel_start, c.sel_end);
  c.text.erase(lo, hi - lo);
  c.cursor = lo;
  c.sel_start = c.sel_end = c.cursor;
}

void ResetBlink(TextInputContent& c) {
  c.blink_timer = 0.0;
  c.cursor_visible = true;
}

void SetValue(TextInputContent& c, const String& text) {
  c.text = text;
  c.cursor = static_cast<u32>(c.text.size());
  c.sel_start = c.sel_end = c.cursor;
}

// --- Event handlers ---------------------------------------------------------

bool TextInputCharInput(WidgetRegistry& world, wid e, u32 codepoint) {
  TextInputContent* cp = world.Get<TextInputContent>(e);
  if (!cp) return false;
  TextInputContent& c = *cp;

  DeleteSelection(c);

  char buf[4];
  u32 len = 0;
  if (codepoint < 0x80) {
    buf[0] = static_cast<char>(codepoint);
    len = 1;
  } else if (codepoint < 0x800) {
    buf[0] = static_cast<char>(0xC0 | (codepoint >> 6));
    buf[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
    len = 2;
  } else if (codepoint < 0x10000) {
    buf[0] = static_cast<char>(0xE0 | (codepoint >> 12));
    buf[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    buf[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
    len = 3;
  } else {
    buf[0] = static_cast<char>(0xF0 | (codepoint >> 18));
    buf[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    buf[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    buf[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
    len = 4;
  }

  c.text.insert(c.cursor, buf, len);
  c.cursor += len;
  c.sel_start = c.sel_end = c.cursor;
  ResetBlink(c);
  MarkDirty(world, e);
  if (c.on_change) c.on_change(c.text);
  return true;
}

bool TextInputKeyDown(WidgetRegistry& world, wid e, i32 key, i32 mods) {
  TextInputContent* cp = world.Get<TextInputContent>(e);
  if (!cp) return false;
  TextInputContent& c = *cp;

  bool shift = (mods & kModShift) != 0;
  bool ctrl = (mods & kModControl) != 0;
  const WidgetContext* ctx = WidgetContextOf(world, e);

  auto move_cursor = [&](u32 new_pos) {
    c.cursor = new_pos;
    if (!shift)
      c.sel_start = c.sel_end = c.cursor;
    else
      c.sel_end = c.cursor;
    ResetBlink(c);
    MarkPaintDirty(world, e);
  };

  switch (key) {
    case kKeyEnter:
    case kKeyKpEnter:
      if (c.on_submit) {
        c.on_submit(c.text);
        return true;
      }
      break;

    case kKeyEscape:
      if (c.on_cancel) {
        c.on_cancel();
        return true;
      }
      break;

    case kKeyUp:
      if (c.on_history_prev) {
        String repl = c.on_history_prev();
        if (repl.size() > 0 || repl != c.text) {
          SetValue(c, repl);
          MarkDirty(world, e);
          if (c.on_change) c.on_change(c.text);
        }
        return true;
      }
      break;

    case kKeyDown:
      if (c.on_history_next) {
        String repl = c.on_history_next();
        SetValue(c, repl);
        MarkDirty(world, e);
        if (c.on_change) c.on_change(c.text);
        return true;
      }
      break;

    case kKeyLeft:
      move_cursor(PrevPos(c.text, c.cursor));
      return true;

    case kKeyRight:
      move_cursor(NextPos(c.text, c.cursor));
      return true;

    case kKeyHome:
      move_cursor(0);
      return true;

    case kKeyEnd:
      move_cursor(static_cast<u32>(c.text.size()));
      return true;

    case kKeyBackspace:
      if (c.sel_start != c.sel_end) {
        DeleteSelection(c);
      } else if (c.cursor > 0) {
        u32 prev = PrevPos(c.text, c.cursor);
        c.text.erase(prev, c.cursor - prev);
        c.cursor = prev;
        c.sel_start = c.sel_end = c.cursor;
      }
      MarkDirty(world, e);
      if (c.on_change) c.on_change(c.text);
      return true;

    case kKeyDelete:
      if (c.sel_start != c.sel_end) {
        DeleteSelection(c);
      } else if (c.cursor < c.text.size()) {
        u32 next = NextPos(c.text, c.cursor);
        c.text.erase(c.cursor, next - c.cursor);
        c.sel_start = c.sel_end = c.cursor;
      }
      MarkDirty(world, e);
      if (c.on_change) c.on_change(c.text);
      return true;

    case kKeyA:
      if (ctrl) {
        c.sel_start = 0;
        c.sel_end = c.cursor = static_cast<u32>(c.text.size());
        MarkPaintDirty(world, e);
        return true;
      }
      break;

    case kKeyC:
      if (ctrl && c.sel_start != c.sel_end && ctx && ctx->platform) {
        u32 lo = std::min(c.sel_start, c.sel_end);
        u32 hi = std::max(c.sel_start, c.sel_end);
        String sel = c.text.substr(lo, hi - lo);
        ctx->platform->set_clipboard_text(sel.c_str());
        return true;
      }
      break;

    case kKeyX:
      if (ctrl && c.sel_start != c.sel_end && ctx && ctx->platform) {
        u32 lo = std::min(c.sel_start, c.sel_end);
        u32 hi = std::max(c.sel_start, c.sel_end);
        String sel = c.text.substr(lo, hi - lo);
        ctx->platform->set_clipboard_text(sel.c_str());
        DeleteSelection(c);
        MarkDirty(world, e);
        if (c.on_change) c.on_change(c.text);
        return true;
      }
      break;

    case kKeyV:
      if (ctrl && ctx && ctx->platform) {
        const char* clip = ctx->platform->clipboard_text();
        if (clip && clip[0]) {
          DeleteSelection(c);
          u32 len = static_cast<u32>(std::strlen(clip));
          c.text.insert(c.cursor, clip, len);
          c.cursor += len;
          c.sel_start = c.sel_end = c.cursor;
          MarkDirty(world, e);
          if (c.on_change) c.on_change(c.text);
        }
        return true;
      }
      break;
  }

  return false;
}

bool TextInputClick(WidgetRegistry& world, wid e) {
  TextInputContent* cp = world.Get<TextInputContent>(e);
  if (!cp) return true;
  TextInputContent& c = *cp;

  TextEngine* te = text_engine(world, e);
  FontHandle fh = effective_font(world, e, c);
  if (!te || fh == kInvalidFont) return true;

  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  auto run = te->Shape(fh, c.text.c_str(), static_cast<u32>(c.text.size()),
                       s.font_size, s.letter_spacing, s.line_height_multiplier);

  const WidgetContext* ctx = WidgetContextOf(world, e);
  Vec2 mp = ctx && ctx->platform
                ? InputToLayoutPoint(world, e,
                                     ctx->platform->input_queue().mouse_pos)
                : Vec2{0, 0};
  Rect content = world.Get<Transform>(e)->content_rect;
  f32 local_x = mp.x - content.x + c.scroll_x;
  c.cursor = PosFromX(c.text, local_x, run);
  c.sel_start = c.sel_end = c.cursor;
  ResetBlink(c);
  MarkPaintDirty(world, e);
  return true;
}

bool TextInputConsumesText(WidgetRegistry& world, wid e) {
  (void)world;
  (void)e;
  return true;
}

// --- Measure & paint --------------------------------------------------------

void TextInputMeasure(WidgetRegistry& world, wid e, f32& out_width,
                      f32& out_height) {
  TextInputContent* c = world.Get<TextInputContent>(e);
  const Style& st = world.Get<StyleC>(e)->style;
  TextEngine* te = text_engine(world, e);
  FontHandle fh = c ? effective_font(world, e, *c) : kInvalidFont;
  if (!c || !te || fh == kInvalidFont) {
    out_width = 0;
    out_height = st.font_size + st.padding.vertical();
    return;
  }

  f32 sc = UiScale(world, e);
  auto run = te->Shape(fh, c->text.empty() ? "X" : c->text.c_str(),
                       c->text.empty() ? 1 : static_cast<u32>(c->text.size()),
                       st.font_size * sc, st.letter_spacing * sc,
                       st.line_height_multiplier);
  out_width = run.total_advance + st.padding.horizontal();
  out_height = run.line_height + st.padding.vertical();
}

void TextInputUpdate(WidgetRegistry& world, wid e, f64 dt) {
  if (!HasState(WidgetStateOf(world, e), WidgetState::kFocused)) return;
  TextInputContent* c = world.Get<TextInputContent>(e);
  if (!c) return;
  c->blink_timer += dt;
  bool was_visible = c->cursor_visible;
  c->cursor_visible = static_cast<int>(c->blink_timer * 2.0) % 2 == 0;
  if (was_visible != c->cursor_visible) MarkPaintDirty(world, e);
}

void TextInputDraw(WidgetRegistry& world, wid e, Renderer2D& renderer) {
  // PaintWidget already drew background / shadow / border.
  TextInputContent* cp = world.Get<TextInputContent>(e);
  if (!cp) return;
  TextInputContent& c = *cp;

  TextEngine* te = text_engine(world, e);
  FontHandle fh = effective_font(world, e, c);
  if (!te || fh == kInvalidFont) return;

  Style s = ComputedStyle(world, e);
  s.Scale(UiScale(world, e));
  f32 alpha = s.opacity;
  bool focused = HasState(WidgetStateOf(world, e), WidgetState::kFocused);
  bool show_placeholder = c.text.empty() && !c.placeholder.empty();

  const String& display = show_placeholder ? c.placeholder : c.text;
  auto run = te->Shape(fh, display.c_str(), static_cast<u32>(display.size()),
                       s.font_size, s.letter_spacing, s.line_height_multiplier);

  Rect content = world.Get<Transform>(e)->content_rect;
  renderer.PushScissor(content);

  f32 text_y = content.y + (content.h - run.line_height) * 0.5f;

  // Scroll to keep the caret visible.
  if (focused && !show_placeholder) {
    f32 cursor_x = CursorXFromPos(c.text, c.cursor, run);
    f32 visible_w = content.w;
    if (cursor_x - c.scroll_x > visible_w - 2.0f)
      c.scroll_x = cursor_x - visible_w + 2.0f;
    if (cursor_x - c.scroll_x < 0.0f) c.scroll_x = cursor_x;
    if (c.scroll_x < 0.0f) c.scroll_x = 0.0f;
  }

  f32 text_x = content.x - c.scroll_x;

  // Selection highlight.
  if (focused && c.sel_start != c.sel_end && !show_placeholder) {
    auto text_run =
        te->Shape(fh, c.text.c_str(), static_cast<u32>(c.text.size()),
                  s.font_size, s.letter_spacing, s.line_height_multiplier);
    u32 lo = std::min(c.sel_start, c.sel_end);
    u32 hi = std::max(c.sel_start, c.sel_end);
    f32 sel_x0 = CursorXFromPos(c.text, lo, text_run);
    f32 sel_x1 = CursorXFromPos(c.text, hi, text_run);
    Color sel_color = {0.3f, 0.5f, 0.9f, 0.4f * alpha};
    renderer.DrawRect(
        {text_x + sel_x0, text_y, sel_x1 - sel_x0, run.line_height}, sel_color);
  }

  Color text_color = show_placeholder
                         ? Color{0.5f, 0.5f, 0.5f, 0.5f * alpha}
                         : s.text_color.WithAlpha(s.text_color.a * alpha);
  renderer.DrawText(Vec2{text_x, text_y}, run, text_color, te->atlas_texture());

  // Caret.
  if (focused && c.cursor_visible && !show_placeholder) {
    auto text_run =
        te->Shape(fh, c.text.c_str(), static_cast<u32>(c.text.size()),
                  s.font_size, s.letter_spacing, s.line_height_multiplier);
    f32 cx = text_x + CursorXFromPos(c.text, c.cursor, text_run);
    Color cursor_color = s.text_color.WithAlpha(s.text_color.a * alpha);
    renderer.DrawRect({cx, text_y + 1.0f, 1.5f, run.line_height - 2.0f},
                      cursor_color);
  }

  renderer.PopScissor();
}

}  // namespace

WidgetVTable TextInputVTable() {
  WidgetVTable vt;
  vt.draw = TextInputDraw;
  vt.measure = TextInputMeasure;
  vt.on_click = TextInputClick;
  vt.on_update = TextInputUpdate;
  vt.on_key_down = TextInputKeyDown;
  vt.on_char_input = TextInputCharInput;
  vt.consumes_text_input = TextInputConsumesText;
  return vt;
}

wid CreateTextInput(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kTextInput;
  world.Add<TextInputContent>(e, TextInputContent{});
  return e;
}

void SetTextInputValue(wid e, const String& text) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kTextInput) return;
  SetValue(world.GetOrAdd<TextInputContent>(e), text);
  MarkDirty(world, e);
}

String TextInputValue(wid e) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kTextInput) return String();
  TextInputContent* c = world.Get<TextInputContent>(e);
  return c ? c->text : String();
}

void SetTextInputPlaceholder(wid e, const String& placeholder) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kTextInput) return;
  world.GetOrAdd<TextInputContent>(e).placeholder = placeholder;
  MarkPaintDirty(world, e);
}

void SetTextInputFont(wid e, FontHandle font) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kTextInput) return;
  world.GetOrAdd<TextInputContent>(e).font = font;
  MarkDirty(world, e);
}

void SetTextInputChange(wid e, TextInputContent::ChangeHandler handler) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kTextInput) return;
  world.GetOrAdd<TextInputContent>(e).on_change = std::move(handler);
}

void SetTextInputSubmit(wid e, TextInputContent::SubmitHandler handler) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kTextInput) return;
  world.GetOrAdd<TextInputContent>(e).on_submit = std::move(handler);
}

void SetTextInputCancel(wid e, TextInputContent::CancelHandler handler) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kTextInput) return;
  world.GetOrAdd<TextInputContent>(e).on_cancel = std::move(handler);
}

void SetTextInputHistoryPrev(wid e, TextInputContent::HistoryHandler handler) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kTextInput) return;
  world.GetOrAdd<TextInputContent>(e).on_history_prev = std::move(handler);
}

void SetTextInputHistoryNext(wid e, TextInputContent::HistoryHandler handler) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  WidgetNode* n = world.Get<WidgetNode>(e);
  if (!n || n->kind != WidgetKind::kTextInput) return;
  world.GetOrAdd<TextInputContent>(e).on_history_next = std::move(handler);
}

}  // namespace ugui
