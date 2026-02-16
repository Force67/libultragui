#ifndef ULTRAGUI_STYLE_ENUMS_H_
#define ULTRAGUI_STYLE_ENUMS_H_

#include <ultragui/core/types.h>

namespace ugui {

/// Interactive state of a widget, used for style selector matching.
/// Multiple flags can be active simultaneously.
enum class WidgetState : u16 {
    kNone = 0,
    kHovered = 1 << 0,
    kPressed = 1 << 1,
    kFocused = 1 << 2,
    kDisabled = 1 << 3,
    kChecked = 1 << 4,
    kSelected = 1 << 5,
    kActive = 1 << 6,
};

constexpr WidgetState operator|(WidgetState a, WidgetState b) {
    return static_cast<WidgetState>(static_cast<u16>(a) | static_cast<u16>(b));
}
constexpr WidgetState operator&(WidgetState a, WidgetState b) {
    return static_cast<WidgetState>(static_cast<u16>(a) & static_cast<u16>(b));
}
constexpr bool HasState(WidgetState flags, WidgetState test) {
    return (static_cast<u16>(flags) & static_cast<u16>(test)) != 0;
}

/// Layout direction
enum class FlexDirection : u8 {
    kRow,
    kColumn,
    kRowReverse,
    kColumnReverse,
};

/// Main-axis alignment
enum class JustifyContent : u8 {
    kStart,
    kEnd,
    kCenter,
    kSpaceBetween,
    kSpaceAround,
    kSpaceEvenly,
};

/// Cross-axis alignment
enum class AlignItems : u8 {
    kStart,
    kEnd,
    kCenter,
    kStretch,
    kBaseline,
};

/// Self-alignment override
enum class AlignSelf : u8 {
    kAuto, // Inherit from parent's AlignItems
    kStart,
    kEnd,
    kCenter,
    kStretch,
};

/// How content overflows its container
enum class Overflow : u8 {
    kVisible,
    kHidden,
    kScroll,
};

/// Text alignment within a text block
enum class TextAlign : u8 {
    kLeft,
    kCenter,
    kRight,
};

/// Positioning mode
enum class Position : u8 {
    kRelative, // Normal flow, offsets are relative to natural position
    kAbsolute, // Removed from flow, positioned relative to parent
};

/// Flex wrapping
enum class FlexWrap : u8 {
    kNoWrap,
    kWrap,
    kWrapReverse,
};

/// Visibility
enum class Visibility : u8 {
    kVisible,
    kHidden,
    kCollapsed, // Hidden + does not take up space
};

/// Text transform (CSS text-transform)
enum class TextTransform : u8 {
    kNone,
    kUppercase,
    kLowercase,
    kCapitalize,
};

/// Cursor style
enum class Cursor : u8 {
    kAuto,
    kDefault,
    kPointer,
    kText,
    kMove,
    kNotAllowed,
};

} // namespace ugui

#endif  // ULTRAGUI_STYLE_ENUMS_H_
