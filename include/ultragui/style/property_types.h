#ifndef ULTRAGUI_STYLE_PROPERTY_TYPES_H_
#define ULTRAGUI_STYLE_PROPERTY_TYPES_H_

#include <ultragui/core/color.h>
#include <ultragui/core/types.h>

namespace ugui {

/// Unit-aware length value for CSS-like properties.
/// Supports pixels, percentages, viewport-relative, fractional, and auto sizing.
struct Length {
    enum class Unit : u8 {
        kPx,      // Absolute pixels
        kPercent, // Percentage of parent's corresponding dimension
        kVw,      // Percentage of viewport width
        kVh,      // Percentage of viewport height
        kFrac,    // Fraction of viewport (0.0-1.0 for the corresponding dimension)
        kAuto,    // Automatic sizing (fit content, fill, etc.)
    };

    f32 value = 0.0f;
    Unit unit = Unit::kPx;

    constexpr Length() = default;
    constexpr Length(f32 v, Unit u) : value(v), unit(u) {}

    static constexpr Length Px(f32 v) { return {v, Unit::kPx}; }
    static constexpr Length Percent(f32 v) { return {v, Unit::kPercent}; }
    static constexpr Length Vw(f32 v) { return {v, Unit::kVw}; }
    static constexpr Length Vh(f32 v) { return {v, Unit::kVh}; }
    static constexpr Length Frac(f32 v) { return {v, Unit::kFrac}; }
    static constexpr Length Auto() { return {0, Unit::kAuto}; }

    constexpr bool IsAuto() const { return unit == Unit::kAuto; }
    constexpr bool IsPercent() const { return unit == Unit::kPercent; }

    /// Resolve to absolute pixels given parent size and viewport size.
    /// For Frac unit, uses viewport_w for width properties and viewport_h for height.
    /// The `for_height` flag selects which viewport dimension Frac maps to.
    f32 Resolve(f32 parent_size, f32 viewport_w, f32 viewport_h,
                bool for_height = false) const {
        switch (unit) {
        case Unit::kPx:
            return value;
        case Unit::kPercent:
            return value * 0.01f * parent_size;
        case Unit::kVw:
            return value * 0.01f * viewport_w;
        case Unit::kVh:
            return value * 0.01f * viewport_h;
        case Unit::kFrac:
            return value * (for_height ? viewport_h : viewport_w);
        case Unit::kAuto:
            return 0.0f;
        }
        return 0.0f;
    }

    constexpr bool operator==(const Length& rhs) const {
        return unit == rhs.unit && (unit == Unit::kAuto || value == rhs.value);
    }
    constexpr bool operator!=(const Length& rhs) const { return !(*this == rhs); }
};

/// Property value that can be one of several types.
/// Used for the generic property system in the IDL parser.
enum class PropType : u8 {
    kNone,
    kLength,
    kColor,
    kNumber,
    kEnum,
    kString,
};

} // namespace ugui

#endif  // ULTRAGUI_STYLE_PROPERTY_TYPES_H_
