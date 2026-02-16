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
    kSticky,   // Stays in flow but clamps to scroll parent's visible area
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

/// Font weight (CSS font-weight numeric scale)
enum class FontWeight : u16 {
    kThin = 100,
    kExtraLight = 200,
    kLight = 300,
    kRegular = 400,
    kMedium = 500,
    kSemiBold = 600,
    kBold = 700,
    kExtraBold = 800,
    kBlack = 900,
};

/// Font style
enum class FontStyle : u8 {
    kNormal,
    kItalic,
};

/// Text transform (CSS text-transform)
enum class TextTransform : u8 {
    kNone,
    kUppercase,
    kLowercase,
    kCapitalize,
};

/// Text decoration (flags - can combine underline + strikethrough)
enum class TextDecoration : u8 {
    kNone = 0,
    kUnderline = 1 << 0,
    kStrikethrough = 1 << 1,
};

constexpr TextDecoration operator|(TextDecoration a, TextDecoration b) {
    return static_cast<TextDecoration>(static_cast<u8>(a) | static_cast<u8>(b));
}
constexpr bool HasDecoration(TextDecoration flags, TextDecoration test) {
    return (static_cast<u8>(flags) & static_cast<u8>(test)) != 0;
}

/// Gradient type
enum class GradientType : u8 {
    kLinear,
    kRadial,
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
