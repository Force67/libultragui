#include <ultragui/style/transition.h>

#include <cmath>

namespace ugui {

static f32 cubic_bezier(f32 p1x, f32 p1y, f32 p2x, f32 p2y, f32 t) {
    // Newton-Raphson to find t_curve for given t on x-axis, then evaluate y
    f32 cx = 3.0f * p1x;
    f32 bx = 3.0f * (p2x - p1x) - cx;
    f32 ax = 1.0f - cx - bx;

    f32 cy = 3.0f * p1y;
    f32 by = 3.0f * (p2y - p1y) - cy;
    f32 ay = 1.0f - cy - by;

    auto sample_x = [ax, bx, cx](f32 s) { return ((ax * s + bx) * s + cx) * s; };
    auto sample_y = [ay, by, cy](f32 s) { return ((ay * s + by) * s + cy) * s; };
    auto sample_dx = [ax, bx, cx](f32 s) { return (3.0f * ax * s + 2.0f * bx) * s + cx; };

    // Solve for s where sample_x(s) == t
    f32 s = t;
    for (int i = 0; i < 8; ++i) {
        f32 x = sample_x(s) - t;
        if (std::abs(x) < 1e-6f)
            break;
        f32 dx = sample_dx(s);
        if (std::abs(dx) < 1e-6f)
            break;
        s -= x / dx;
    }
    s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
    return sample_y(s);
}

f32 EvalEasing(EasingType type, f32 t, const f32 bezier[4]) {
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

    switch (type) {
    case EasingType::kLinear:
        return t;
    case EasingType::kEaseIn:
        return t * t * t;
    case EasingType::kEaseOut: {
        f32 inv = 1.0f - t;
        return 1.0f - inv * inv * inv;
    }
    case EasingType::kEaseInOut:
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - 0.5f * std::pow(-2.0f * t + 2.0f, 3.0f);
    case EasingType::kCubicBezier:
        if (bezier)
            return cubic_bezier(bezier[0], bezier[1], bezier[2], bezier[3], t);
        return t;
    }
    return t;
}

} // namespace ugui
