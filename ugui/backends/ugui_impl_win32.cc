// ugui_impl_win32: see ugui_impl_win32.h.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>

#include <ugui/backends/ugui_impl_win32.h>
#include <ugui/platform/platform_host.h>

namespace ugui {
namespace win32 {
namespace {

// GLFW's key codes, the ones this file produces.
constexpr i32 kUnknown = -1;
constexpr i32 kEscape = 256;
constexpr i32 kEnter = 257;
constexpr i32 kTab = 258;
constexpr i32 kBackspace = 259;
constexpr i32 kInsert = 260;
constexpr i32 kDelete = 261;
constexpr i32 kRight = 262;
constexpr i32 kLeft = 263;
constexpr i32 kDown = 264;
constexpr i32 kUp = 265;
constexpr i32 kPageUp = 266;
constexpr i32 kPageDown = 267;
constexpr i32 kHome = 268;
constexpr i32 kEnd = 269;
constexpr i32 kCapsLock = 280;
constexpr i32 kScrollLock = 281;
constexpr i32 kNumLock = 282;
constexpr i32 kPrintScreen = 283;
constexpr i32 kPause = 284;
constexpr i32 kF1 = 290;
constexpr i32 kKp0 = 320;
constexpr i32 kKpDecimal = 330;
constexpr i32 kKpDivide = 331;
constexpr i32 kKpMultiply = 332;
constexpr i32 kKpSubtract = 333;
constexpr i32 kKpAdd = 334;
constexpr i32 kKpEnter = 335;
constexpr i32 kLeftShift = 340;
constexpr i32 kLeftControl = 341;
constexpr i32 kLeftAlt = 342;
constexpr i32 kLeftSuper = 343;
constexpr i32 kRightShift = 344;
constexpr i32 kRightControl = 345;
constexpr i32 kRightAlt = 346;
constexpr i32 kRightSuper = 347;
constexpr i32 kMenu = 348;

constexpr i32 kModShift = 0x1;
constexpr i32 kModControl = 0x2;
constexpr i32 kModAlt = 0x4;
constexpr i32 kModSuper = 0x8;
constexpr i32 kModCapsLock = 0x10;
constexpr i32 kModNumLock = 0x20;

bool Extended(intptr_t lparam) { return (HIWORD(lparam) & KF_EXTENDED) != 0; }

Vec2 MousePosition(intptr_t lparam, Vec2 scale) {
  return {static_cast<f32>(GET_X_LPARAM(lparam)) * scale.x,
          static_cast<f32>(GET_Y_LPARAM(lparam)) * scale.y};
}

// A UTF-16 unit at a time; a surrogate pair arrives as two WM_CHARs.
WCHAR g_high_surrogate = 0;

}  // namespace

i32 KeyFromVirtualKey(u32 vk, intptr_t lparam) {
  if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
    return static_cast<i32>(vk);
  if (vk >= VK_F1 && vk <= VK_F24) return kF1 + static_cast<i32>(vk - VK_F1);
  if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
    return kKp0 + static_cast<i32>(vk - VK_NUMPAD0);
  switch (vk) {
    case VK_SPACE: return 32;
    case VK_OEM_7: return 39;       // '
    case VK_OEM_COMMA: return 44;   // ,
    case VK_OEM_MINUS: return 45;   // -
    case VK_OEM_PERIOD: return 46;  // .
    case VK_OEM_2: return 47;       // /
    case VK_OEM_1: return 59;       // ;
    case VK_OEM_PLUS: return 61;    // =
    case VK_OEM_4: return 91;       // [
    case VK_OEM_5: return 92;       // backslash
    case VK_OEM_6: return 93;       // ]
    case VK_OEM_3: return 96;       // `
    case VK_ESCAPE: return kEscape;
    case VK_RETURN: return Extended(lparam) ? kKpEnter : kEnter;
    case VK_TAB: return kTab;
    case VK_BACK: return kBackspace;
    case VK_INSERT: return kInsert;
    case VK_DELETE: return kDelete;
    case VK_RIGHT: return kRight;
    case VK_LEFT: return kLeft;
    case VK_DOWN: return kDown;
    case VK_UP: return kUp;
    case VK_PRIOR: return kPageUp;
    case VK_NEXT: return kPageDown;
    case VK_HOME: return kHome;
    case VK_END: return kEnd;
    case VK_CAPITAL: return kCapsLock;
    case VK_SCROLL: return kScrollLock;
    case VK_NUMLOCK: return kNumLock;
    case VK_SNAPSHOT: return kPrintScreen;
    case VK_PAUSE: return kPause;
    case VK_DECIMAL: return kKpDecimal;
    case VK_DIVIDE: return kKpDivide;
    case VK_MULTIPLY: return kKpMultiply;
    case VK_SUBTRACT: return kKpSubtract;
    case VK_ADD: return kKpAdd;
    case VK_LWIN: return kLeftSuper;
    case VK_RWIN: return kRightSuper;
    case VK_APPS: return kMenu;
    // The generic modifiers; the scan code says which side.
    case VK_SHIFT:
      return MapVirtualKeyW((static_cast<UINT>(lparam) >> 16) & 0xFF,
                            MAPVK_VSC_TO_VK_EX) == VK_RSHIFT
                 ? kRightShift
                 : kLeftShift;
    case VK_CONTROL: return Extended(lparam) ? kRightControl : kLeftControl;
    case VK_MENU: return Extended(lparam) ? kRightAlt : kLeftAlt;
    case VK_LSHIFT: return kLeftShift;
    case VK_RSHIFT: return kRightShift;
    case VK_LCONTROL: return kLeftControl;
    case VK_RCONTROL: return kRightControl;
    case VK_LMENU: return kLeftAlt;
    case VK_RMENU: return kRightAlt;
    default: return kUnknown;
  }
}

i32 CurrentMods() {
  i32 mods = 0;
  if (GetKeyState(VK_SHIFT) & 0x8000) mods |= kModShift;
  if (GetKeyState(VK_CONTROL) & 0x8000) mods |= kModControl;
  if (GetKeyState(VK_MENU) & 0x8000) mods |= kModAlt;
  if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mods |= kModSuper;
  if (GetKeyState(VK_CAPITAL) & 0x1) mods |= kModCapsLock;
  if (GetKeyState(VK_NUMLOCK) & 0x1) mods |= kModNumLock;
  return mods;
}

bool HandleMessage(Platform& platform, void* window, u32 message,
                   uintptr_t wparam, intptr_t lparam, Vec2 scale) {
  switch (message) {
    case WM_MOUSEMOVE:
      host::PushMouseMove(platform, MousePosition(lparam, scale));
      return true;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
      const bool pressed = message != WM_LBUTTONUP &&
                           message != WM_RBUTTONUP && message != WM_MBUTTONUP;
      MouseButton button = MouseButton::kLeft;
      if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK ||
          message == WM_RBUTTONUP)
        button = MouseButton::kRight;
      else if (message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK ||
               message == WM_MBUTTONUP)
        button = MouseButton::kMiddle;
      // A drag that leaves the window still ends here.
      if (window) {
        if (pressed)
          SetCapture(static_cast<HWND>(window));
        else if (GetCapture() == static_cast<HWND>(window))
          ReleaseCapture();
      }
      host::PushMouseMove(platform, MousePosition(lparam, scale));
      host::PushMouseButton(platform, button, pressed);
      return true;
    }
    case WM_MOUSEWHEEL:
      host::PushScroll(platform,
                       {0.0f, static_cast<f32>(GET_WHEEL_DELTA_WPARAM(wparam)) /
                                  static_cast<f32>(WHEEL_DELTA)});
      return true;
    case WM_MOUSEHWHEEL:
      host::PushScroll(platform,
                       {-static_cast<f32>(GET_WHEEL_DELTA_WPARAM(wparam)) /
                            static_cast<f32>(WHEEL_DELTA),
                        0.0f});
      return true;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
      const i32 key = KeyFromVirtualKey(static_cast<u32>(wparam), lparam);
      const i32 scancode = static_cast<i32>((static_cast<uintptr_t>(lparam) >> 16) & 0x1FF);
      const bool repeat = pressed && (lparam & (1 << 30)) != 0;
      host::PushKey(platform, key, scancode, pressed, repeat, CurrentMods());
      return true;
    }
    case WM_CHAR: {
      const WCHAR unit = static_cast<WCHAR>(wparam);
      if (IS_HIGH_SURROGATE(unit)) {
        g_high_surrogate = unit;
        return true;
      }
      u32 codepoint = unit;
      if (IS_LOW_SURROGATE(unit) && g_high_surrogate != 0)
        codepoint = 0x10000 + ((static_cast<u32>(g_high_surrogate) - 0xD800) << 10) +
                    (static_cast<u32>(unit) - 0xDC00);
      g_high_surrogate = 0;
      // Control characters arrive as keys already.
      if (codepoint >= 32 && codepoint != 127)
        host::PushChar(platform, codepoint);
      return true;
    }
    default:
      return false;
  }
}

}  // namespace win32
}  // namespace ugui
