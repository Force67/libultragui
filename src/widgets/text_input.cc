#include <ultragui/platform/platform.h>
#include <ultragui/render/renderer2d.h>
#include <ultragui/render/vertex.h>
#include <ultragui/widgets/text_input.h>

#include <algorithm>
#include <cstring>

// GLFW key codes (avoid including GLFW in widget code)
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

// ---------------------------------------------------------------------------
// UTF-8 navigation
// ---------------------------------------------------------------------------

u32 TextInput::NextPos(u32 pos) const {
  if (pos >= text_.size()) return static_cast<u32>(text_.size());
  u8 c = static_cast<u8>(text_[pos]);
  if (c < 0x80) return pos + 1;
  if ((c & 0xE0) == 0xC0)
    return std::min(pos + 2, static_cast<u32>(text_.size()));
  if ((c & 0xF0) == 0xE0)
    return std::min(pos + 3, static_cast<u32>(text_.size()));
  return std::min(pos + 4, static_cast<u32>(text_.size()));
}

u32 TextInput::PrevPos(u32 pos) const {
  if (pos == 0) return 0;
  --pos;
  while (pos > 0 && (static_cast<u8>(text_[pos]) & 0xC0) == 0x80) --pos;
  return pos;
}

// ---------------------------------------------------------------------------
// Cursor <-> glyph position mapping
// ---------------------------------------------------------------------------

f32 TextInput::CursorXFromPos(u32 byte_pos, const TextRun& run) const {
  // Sum x_advances for glyphs whose source bytes precede byte_pos.
  // For a simple implementation, assume 1 glyph per codepoint.
  f32 x = 0.0f;
  u32 src_pos = 0;
  for (u32 i = 0; i < run.glyph_count && src_pos < byte_pos; ++i) {
    x += run.glyphs[i].x_advance;
    src_pos = NextPos(src_pos);
  }
  return x;
}

u32 TextInput::PosFromX(f32 local_x, const TextRun& run) const {
  f32 x = 0.0f;
  u32 src_pos = 0;
  for (u32 i = 0; i < run.glyph_count; ++i) {
    f32 mid = x + run.glyphs[i].x_advance * 0.5f;
    if (local_x < mid) return src_pos;
    x += run.glyphs[i].x_advance;
    src_pos = NextPos(src_pos);
  }
  return static_cast<u32>(text_.size());
}

// ---------------------------------------------------------------------------
// Selection helpers
// ---------------------------------------------------------------------------

void TextInput::DeleteSelection() {
  if (sel_start_ == sel_end_) return;
  u32 lo = std::min(sel_start_, sel_end_);
  u32 hi = std::max(sel_start_, sel_end_);
  text_.erase(lo, hi - lo);
  cursor_ = lo;
  sel_start_ = sel_end_ = cursor_;
}

void TextInput::ResetBlink() {
  blink_timer_ = 0.0;
  cursor_visible_ = true;
}

void TextInput::set_text(const String& text) {
  text_ = text;
  cursor_ = static_cast<u32>(text_.size());
  sel_start_ = sel_end_ = cursor_;
  MarkDirty();
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

bool TextInput::OnCharInput(u32 codepoint) {
  DeleteSelection();

  // Encode codepoint as UTF-8
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

  text_.insert(cursor_, buf, len);
  cursor_ += len;
  sel_start_ = sel_end_ = cursor_;
  ResetBlink();
  MarkDirty();
  if (on_change_) on_change_(text_);
  return true;
}

bool TextInput::OnKeyDown(i32 key, i32 mods) {
  bool shift = (mods & kModShift) != 0;
  bool ctrl = (mods & kModControl) != 0;

  auto move_cursor = [&](u32 new_pos) {
    cursor_ = new_pos;
    if (!shift)
      sel_start_ = sel_end_ = cursor_;
    else
      sel_end_ = cursor_;
    ResetBlink();
    MarkPaintDirty();
  };

  switch (key) {
    case kKeyEnter:
    case kKeyKpEnter:
      if (on_submit_) {
        on_submit_(text_);
        return true;
      }
      break;

    case kKeyEscape:
      if (on_cancel_) {
        on_cancel_();
        return true;
      }
      break;

    case kKeyUp:
      if (on_history_prev_) {
        String repl = on_history_prev_();
        if (repl.size() > 0 || repl != text_) {
          set_text(repl);
          if (on_change_) on_change_(text_);
        }
        return true;
      }
      break;

    case kKeyDown:
      if (on_history_next_) {
        String repl = on_history_next_();
        set_text(repl);
        if (on_change_) on_change_(text_);
        return true;
      }
      break;

    case kKeyLeft:
      move_cursor(PrevPos(cursor_));
      return true;

    case kKeyRight:
      move_cursor(NextPos(cursor_));
      return true;

    case kKeyHome:
      move_cursor(0);
      return true;

    case kKeyEnd:
      move_cursor(static_cast<u32>(text_.size()));
      return true;

    case kKeyBackspace:
      if (sel_start_ != sel_end_) {
        DeleteSelection();
      } else if (cursor_ > 0) {
        u32 prev = PrevPos(cursor_);
        text_.erase(prev, cursor_ - prev);
        cursor_ = prev;
        sel_start_ = sel_end_ = cursor_;
      }
      MarkDirty();
      if (on_change_) on_change_(text_);
      return true;

    case kKeyDelete:
      if (sel_start_ != sel_end_) {
        DeleteSelection();
      } else if (cursor_ < text_.size()) {
        u32 next = NextPos(cursor_);
        text_.erase(cursor_, next - cursor_);
        sel_start_ = sel_end_ = cursor_;
      }
      MarkDirty();
      if (on_change_) on_change_(text_);
      return true;

    case kKeyA:
      if (ctrl) {
        sel_start_ = 0;
        sel_end_ = cursor_ = static_cast<u32>(text_.size());
        MarkPaintDirty();
        return true;
      }
      break;

    case kKeyC:
      if (ctrl && sel_start_ != sel_end_ && context_ && context_->platform) {
        u32 lo = std::min(sel_start_, sel_end_);
        u32 hi = std::max(sel_start_, sel_end_);
        String sel = text_.substr(lo, hi - lo);
        context_->platform->set_clipboard_text(sel.c_str());
        return true;
      }
      break;

    case kKeyX:
      if (ctrl && sel_start_ != sel_end_ && context_ && context_->platform) {
        u32 lo = std::min(sel_start_, sel_end_);
        u32 hi = std::max(sel_start_, sel_end_);
        String sel = text_.substr(lo, hi - lo);
        context_->platform->set_clipboard_text(sel.c_str());
        DeleteSelection();
        MarkDirty();
        if (on_change_) on_change_(text_);
        return true;
      }
      break;

    case kKeyV:
      if (ctrl && context_ && context_->platform) {
        const char* clip = context_->platform->clipboard_text();
        if (clip && clip[0]) {
          DeleteSelection();
          u32 len = static_cast<u32>(std::strlen(clip));
          text_.insert(cursor_, clip, len);
          cursor_ += len;
          sel_start_ = sel_end_ = cursor_;
          MarkDirty();
          if (on_change_) on_change_(text_);
        }
        return true;
      }
      break;
  }

  return false;
}

bool TextInput::OnClick() {
  // Position cursor from mouse click
  auto* te = text_engine();
  FontHandle fh = effective_font();
  if (!te || fh == kInvalidFont) return true;

  auto s = ComputedStyle();
  s.Scale(ui_scale());
  auto run = te->Shape(fh, text_.c_str(), static_cast<u32>(text_.size()),
                       s.font_size, s.letter_spacing, s.line_height_multiplier);

  // Mouse position relative to text origin
  Vec2 mp =
      context_ && context_->platform
          ? InputToLayoutPoint(context_->platform->input_queue().mouse_pos)
          : Vec2{0, 0};
  f32 local_x = mp.x - content_rect_.x + scroll_x_;
  cursor_ = PosFromX(local_x, run);
  sel_start_ = sel_end_ = cursor_;
  ResetBlink();
  MarkPaintDirty();
  return true;
}

// ---------------------------------------------------------------------------
// Measure & Paint
// ---------------------------------------------------------------------------

void TextInput::Measure(f32& out_width, f32& out_height) {
  auto* te = text_engine();
  FontHandle fh = effective_font();
  if (!te || fh == kInvalidFont) {
    out_width = 0;
    out_height = style_.font_size + style_.padding.vertical();
    return;
  }

  f32 sc = ui_scale();
  auto run = te->Shape(fh, text_.empty() ? "X" : text_.c_str(),
                       text_.empty() ? 1 : static_cast<u32>(text_.size()),
                       style_.font_size * sc, style_.letter_spacing * sc,
                       style_.line_height_multiplier);
  out_width = run.total_advance + style_.padding.horizontal();
  out_height = run.line_height + style_.padding.vertical();
}

void TextInput::OnUpdate(f64 dt) {
  // Cursor blink (only when focused)
  if (HasState(state_, WidgetState::kFocused)) {
    blink_timer_ += dt;
    bool was_visible = cursor_visible_;
    cursor_visible_ = static_cast<int>(blink_timer_ * 2.0) % 2 == 0;
    if (was_visible != cursor_visible_) MarkPaintDirty();
  }
}

void TextInput::OnPaint(Renderer2D& renderer) {
  Widget::OnPaint(renderer);  // Background, shadow, border

  auto* te = text_engine();
  FontHandle fh = effective_font();
  if (!te || fh == kInvalidFont) return;

  auto s = ComputedStyle();
  s.Scale(ui_scale());
  f32 alpha = s.opacity;
  bool focused = HasState(state_, WidgetState::kFocused);
  bool show_placeholder = text_.empty() && !placeholder_.empty();

  const String& display = show_placeholder ? placeholder_ : text_;
  auto run = te->Shape(fh, display.c_str(), static_cast<u32>(display.size()),
                       s.font_size, s.letter_spacing, s.line_height_multiplier);

  // Clip to content rect
  renderer.PushScissor(content_rect_);

  // Compute text origin
  f32 text_y = content_rect_.y + (content_rect_.h - run.line_height) * 0.5f;

  // Scroll to keep cursor visible
  if (focused && !show_placeholder) {
    f32 cursor_x = CursorXFromPos(cursor_, run);
    f32 visible_w = content_rect_.w;
    if (cursor_x - scroll_x_ > visible_w - 2.0f)
      scroll_x_ = cursor_x - visible_w + 2.0f;
    if (cursor_x - scroll_x_ < 0.0f) scroll_x_ = cursor_x;
    if (scroll_x_ < 0.0f) scroll_x_ = 0.0f;
  }

  f32 text_x = content_rect_.x - scroll_x_;

  // Draw selection highlight
  if (focused && sel_start_ != sel_end_ && !show_placeholder) {
    // Shape the actual text for selection positioning
    auto text_run =
        te->Shape(fh, text_.c_str(), static_cast<u32>(text_.size()),
                  s.font_size, s.letter_spacing, s.line_height_multiplier);
    u32 lo = std::min(sel_start_, sel_end_);
    u32 hi = std::max(sel_start_, sel_end_);
    f32 sel_x0 = CursorXFromPos(lo, text_run);
    f32 sel_x1 = CursorXFromPos(hi, text_run);
    Color sel_color = {0.3f, 0.5f, 0.9f, 0.4f * alpha};
    renderer.DrawRect(
        {text_x + sel_x0, text_y, sel_x1 - sel_x0, run.line_height}, sel_color);
  }

  // Draw text
  Color text_color = show_placeholder
                         ? Color{0.5f, 0.5f, 0.5f, 0.5f * alpha}
                         : s.text_color.WithAlpha(s.text_color.a * alpha);
  renderer.DrawText(Vec2{text_x, text_y}, run, text_color, te->atlas_texture());

  // Draw cursor
  if (focused && cursor_visible_ && !show_placeholder) {
    auto text_run =
        te->Shape(fh, text_.c_str(), static_cast<u32>(text_.size()),
                  s.font_size, s.letter_spacing, s.line_height_multiplier);
    f32 cx = text_x + CursorXFromPos(cursor_, text_run);
    Color cursor_color = s.text_color.WithAlpha(s.text_color.a * alpha);
    renderer.DrawRect({cx, text_y + 1.0f, 1.5f, run.line_height - 2.0f},
                      cursor_color);
  }

  renderer.PopScissor();
}

}  // namespace ugui
