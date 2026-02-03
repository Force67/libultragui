#pragma once

#include <ultragui/core/types.h>

namespace ugui {

/// Interactive state of a widget, used for style selector matching.
/// Multiple flags can be active simultaneously.
enum class WidgetState : u16 {
    None = 0,
    Hovered = 1 << 0,
    Pressed = 1 << 1,
    Focused = 1 << 2,
    Disabled = 1 << 3,
    Checked = 1 << 4,
    Selected = 1 << 5,
    Active = 1 << 6,
};

constexpr WidgetState operator|(WidgetState a, WidgetState b) {
    return static_cast<WidgetState>(static_cast<u16>(a) | static_cast<u16>(b));
}
constexpr WidgetState operator&(WidgetState a, WidgetState b) {
    return static_cast<WidgetState>(static_cast<u16>(a) & static_cast<u16>(b));
}
constexpr bool has_state(WidgetState flags, WidgetState test) {
    return (static_cast<u16>(flags) & static_cast<u16>(test)) != 0;
}

/// Layout direction
enum class FlexDirection : u8 {
    Row,
    Column,
    RowReverse,
    ColumnReverse,
};

/// Main-axis alignment
enum class JustifyContent : u8 {
    Start,
    End,
    Center,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly,
};

/// Cross-axis alignment
enum class AlignItems : u8 {
    Start,
    End,
    Center,
    Stretch,
    Baseline,
};

/// Self-alignment override
enum class AlignSelf : u8 {
    Auto, // Inherit from parent's AlignItems
    Start,
    End,
    Center,
    Stretch,
};

/// How content overflows its container
enum class Overflow : u8 {
    Visible,
    Hidden,
    Scroll,
};

/// Text alignment within a text block
enum class TextAlign : u8 {
    Left,
    Center,
    Right,
};

/// Positioning mode
enum class Position : u8 {
    Relative, // Normal flow, offsets are relative to natural position
    Absolute, // Removed from flow, positioned relative to parent
};

/// Flex wrapping
enum class FlexWrap : u8 {
    NoWrap,
    Wrap,
    WrapReverse,
};

/// Visibility
enum class Visibility : u8 {
    Visible,
    Hidden,
    Collapsed, // Hidden + does not take up space
};

/// Text transform (CSS text-transform)
enum class TextTransform : u8 {
    None,
    Uppercase,
    Lowercase,
    Capitalize,
};

/// Cursor style
enum class Cursor : u8 {
    Auto,
    Default,
    Pointer,
    Text,
    Move,
    NotAllowed,
};

} // namespace ugui
