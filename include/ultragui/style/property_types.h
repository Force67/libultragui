#pragma once

#include <ultragui/core/color.h>
#include <ultragui/core/types.h>

namespace ugui {

/// Unit-aware length value for CSS-like properties.
/// Supports pixels, percentages, viewport-relative, fractional, and auto sizing.
struct Length {
    enum class Unit : u8 {
        Px,      // Absolute pixels
        Percent, // Percentage of parent's corresponding dimension
        Vw,      // Percentage of viewport width
        Vh,      // Percentage of viewport height
        Frac,    // Fraction of viewport (0.0-1.0 for the corresponding dimension)
        Auto,    // Automatic sizing (fit content, fill, etc.)
    };

    f32 value = 0.0f;
    Unit unit = Unit::Px;

    constexpr Length() = default;
    constexpr Length(f32 v, Unit u) : value(v), unit(u) {}

    static constexpr Length px(f32 v) { return {v, Unit::Px}; }
    static constexpr Length percent(f32 v) { return {v, Unit::Percent}; }
    static constexpr Length vw(f32 v) { return {v, Unit::Vw}; }
    static constexpr Length vh(f32 v) { return {v, Unit::Vh}; }
    static constexpr Length frac(f32 v) { return {v, Unit::Frac}; }
    static constexpr Length auto_() { return {0, Unit::Auto}; }

    constexpr bool is_auto() const { return unit == Unit::Auto; }
    constexpr bool is_percent() const { return unit == Unit::Percent; }

    /// Resolve to absolute pixels given parent size and viewport size.
    /// For Frac unit, uses viewport_w for width properties and viewport_h for height.
    /// The `for_height` flag selects which viewport dimension Frac maps to.
    f32 resolve(f32 parent_size, f32 viewport_w, f32 viewport_h,
                bool for_height = false) const {
        switch (unit) {
        case Unit::Px:
            return value;
        case Unit::Percent:
            return value * 0.01f * parent_size;
        case Unit::Vw:
            return value * 0.01f * viewport_w;
        case Unit::Vh:
            return value * 0.01f * viewport_h;
        case Unit::Frac:
            return value * (for_height ? viewport_h : viewport_w);
        case Unit::Auto:
            return 0.0f;
        }
        return 0.0f;
    }

    constexpr bool operator==(const Length& rhs) const {
        return unit == rhs.unit && (unit == Unit::Auto || value == rhs.value);
    }
    constexpr bool operator!=(const Length& rhs) const { return !(*this == rhs); }
};

/// Property value that can be one of several types.
/// Used for the generic property system in the IDL parser.
enum class PropType : u8 {
    None,
    Length,
    Color,
    Number,
    Enum,
    String,
};

} // namespace ugui
