// A windowless ugui::Platform the host drives. See platform_host.h.
#include <ugui/platform/platform.h>
#include <ugui/platform/platform_host.h>

#if defined(ULTRAGUI_USE_BASE)
#include <base/threading/lock_guard.h>
#include <base/threading/mutex.h>
#include <base/time/time.h>
#else
#include <chrono>
#include <mutex>
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <ugui/core/config.h>

namespace ugui {

namespace {
#if defined(ULTRAGUI_USE_BASE)
using HostMutex = base::Mutex;
using HostLock = base::LockGuard<base::Mutex>;
using HostTime = base::TimeTicks;
HostTime HostNow() { return base::TimeTicks::Now(); }
f64 SecondsSince(HostTime start) { return (HostNow() - start).InSecondsF(); }
#else
using HostMutex = std::mutex;
using HostLock = std::lock_guard<std::mutex>;
using HostTime = std::chrono::steady_clock::time_point;
HostTime HostNow() { return std::chrono::steady_clock::now(); }
f64 SecondsSince(HostTime start) {
  return std::chrono::duration<f64>(HostNow() - start).count();
}
#endif
}  // namespace

struct Platform::Impl {
  HostMutex lock;
  // Filled by the host between polls; handed to `queue` by PollEvents.
  InputQueue pending;
  // What the input router reads this frame.
  InputQueue queue;
  Vec2 window_size = {0.0f, 0.0f};
  Vec2 framebuffer_size = {0.0f, 0.0f};
  Cursor cursor = Cursor::kDefault;
  HostTime start;
  String clipboard;
};

Platform::Platform() : impl_(new Impl()) {}
Platform::~Platform() { delete impl_; }

bool Platform::Init(const WindowConfig& config) {
  impl_->start = HostNow();
  const Vec2 size = {static_cast<f32>(config.width),
                     static_cast<f32>(config.height)};
  impl_->window_size = size;
  impl_->framebuffer_size = size;
  return true;
}

void Platform::Shutdown() {}

bool Platform::ShouldClose() const { return false; }

void Platform::PollEvents() {
  HostLock hold(impl_->lock);
  impl_->queue = impl_->pending;
  impl_->pending.clear();
  // The pointer stays where it last was, with or without new events.
  impl_->pending.mouse_pos = impl_->queue.mouse_pos;
}

Vec2 Platform::window_size() const {
  HostLock hold(impl_->lock);
  return impl_->window_size;
}

Vec2 Platform::framebuffer_size() const {
  HostLock hold(impl_->lock);
  return impl_->framebuffer_size;
}

f32 Platform::dpi_scale() const {
  HostLock hold(impl_->lock);
  return impl_->window_size.x > 0.0f
             ? impl_->framebuffer_size.x / impl_->window_size.x
             : 1.0f;
}

f64 Platform::time() const {
  return SecondsSince(impl_->start);
}

void* Platform::native_handle() const { return nullptr; }

void Platform::SetCursor(Cursor cursor) {
  HostLock hold(impl_->lock);
  impl_->cursor = cursor;
}

#if defined(_WIN32)
// The system clipboard, as UTF-8. The returned text lives until the next call.
const char* Platform::clipboard_text() const {
  impl_->clipboard.clear();
  if (!OpenClipboard(nullptr)) return impl_->clipboard.c_str();
  if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
    if (const wchar_t* wide = static_cast<const wchar_t*>(GlobalLock(data))) {
      const int bytes =
          WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
      if (bytes > 1) {
        impl_->clipboard.resize(static_cast<size_t>(bytes - 1));
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, &impl_->clipboard[0], bytes,
                            nullptr, nullptr);
      }
      GlobalUnlock(data);
    }
  }
  CloseClipboard();
  return impl_->clipboard.c_str();
}

void Platform::set_clipboard_text(const char* text) {
  if (!text) return;
  const int wide_count = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  if (wide_count <= 0 || !OpenClipboard(nullptr)) return;
  EmptyClipboard();
  HGLOBAL memory =
      GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wide_count) * sizeof(wchar_t));
  if (memory) {
    if (wchar_t* wide = static_cast<wchar_t*>(GlobalLock(memory))) {
      MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_count);
      GlobalUnlock(memory);
      if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
    } else {
      GlobalFree(memory);
    }
  }
  CloseClipboard();
}
#else
// No system clipboard to reach without a window system: one kept here.
const char* Platform::clipboard_text() const { return impl_->clipboard.c_str(); }
void Platform::set_clipboard_text(const char* text) {
  impl_->clipboard = text ? text : "";
}
#endif

InputQueue& Platform::input_queue() { return impl_->queue; }

namespace host {

void SetViewport(Platform& platform, Vec2 window_size, Vec2 framebuffer_size) {
  Platform::Impl& impl = *platform.impl();
  HostLock hold(impl.lock);
  impl.window_size = window_size;
  impl.framebuffer_size = framebuffer_size;
}

void PushMouseMove(Platform& platform, Vec2 position) {
  Platform::Impl& impl = *platform.impl();
  HostLock hold(impl.lock);
  impl.pending.PushMove(position);
}

void PushMouseButton(Platform& platform, MouseButton button, bool pressed) {
  Platform::Impl& impl = *platform.impl();
  HostLock hold(impl.lock);
  impl.pending.PushButton(button, pressed);
}

void PushScroll(Platform& platform, Vec2 delta) {
  Platform::Impl& impl = *platform.impl();
  HostLock hold(impl.lock);
  impl.pending.PushScroll(delta);
}

void PushKey(Platform& platform, i32 key, i32 scancode, bool pressed,
             bool repeat, i32 mods) {
  Platform::Impl& impl = *platform.impl();
  HostLock hold(impl.lock);
  impl.pending.PushKey(key, scancode, pressed, repeat, mods);
}

void PushChar(Platform& platform, u32 codepoint) {
  Platform::Impl& impl = *platform.impl();
  HostLock hold(impl.lock);
  impl.pending.PushChar(codepoint);
}

Cursor RequestedCursor(const Platform& platform) {
  Platform::Impl& impl = *platform.impl();
  HostLock hold(impl.lock);
  return impl.cursor;
}

}  // namespace host
}  // namespace ugui
