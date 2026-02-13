#include "svg_types.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ugui {
namespace svg {

// ============================================================================
// Minimal XML tokenizer
// ============================================================================

struct XmlAttr {
    std::string name;
    std::string value;
};

struct XmlNode {
    std::string tag;
    std::vector<XmlAttr> attrs;
    std::vector<XmlNode> children;

    const char* attr(const char* name) const {
        for (auto& a : attrs)
            if (a.name == name)
                return a.value.c_str();
        return nullptr;
    }

    f32 attr_f(const char* name, f32 def = 0) const {
        auto* v = attr(name);
        return v ? static_cast<f32>(std::atof(v)) : def;
    }

    const XmlNode* child(const char* tag_name) const {
        for (auto& c : children)
            if (c.tag == tag_name)
                return &c;
        return nullptr;
    }
};

struct XmlParser {
    const char* p;
    const char* end;

    bool eof() const { return p >= end; }
    char peek() const { return eof() ? 0 : *p; }
    char next() { return eof() ? 0 : *p++; }

    void skip_ws() {
        while (!eof() && std::isspace(static_cast<u8>(*p)))
            ++p;
    }

    void skip_until(const char* seq) {
        usize len = std::strlen(seq);
        while (!eof()) {
            if (static_cast<usize>(end - p) >= len && std::memcmp(p, seq, len) == 0) {
                p += len;
                return;
            }
            ++p;
        }
    }

    std::string read_name() {
        const char* start = p;
        while (!eof() && (std::isalnum(static_cast<u8>(*p)) || *p == '-' || *p == '_' ||
                          *p == ':' || *p == '.'))
            ++p;
        return std::string(start, p);
    }

    std::string read_quoted() {
        char q = next(); // consume opening quote
        const char* start = p;
        while (!eof() && *p != q)
            ++p;
        std::string result(start, p);
        if (!eof())
            ++p; // consume closing quote
        return result;
    }

    bool parse_node(XmlNode& node) {
        skip_ws();
        if (eof() || peek() != '<')
            return false;
        ++p; // skip '<'

        // Skip comments, CDATA, processing instructions, DOCTYPE
        if (peek() == '!') {
            if (end - p >= 2 && p[0] == '!' && p[1] == '-') {
                skip_until("-->");
                return false; // signal: skip, not a real node
            }
            // DOCTYPE or CDATA
            skip_until(">");
            return false;
        }
        if (peek() == '?') {
            skip_until("?>");
            return false;
        }
        if (peek() == '/') {
            // Close tag - caller handles this
            return false;
        }

        node.tag = read_name();
        if (node.tag.empty())
            return false;

        // Parse attributes
        for (;;) {
            skip_ws();
            if (eof())
                break;
            if (peek() == '/' || peek() == '>')
                break;
            XmlAttr attr;
            attr.name = read_name();
            if (attr.name.empty()) {
                ++p;
                continue;
            }
            skip_ws();
            if (peek() == '=') {
                ++p;
                skip_ws();
                if (peek() == '"' || peek() == '\'')
                    attr.value = read_quoted();
            }
            node.attrs.push_back(std::move(attr));
        }

        bool self_closing = false;
        if (peek() == '/') {
            self_closing = true;
            ++p;
        }
        if (peek() == '>')
            ++p;

        if (self_closing)
            return true;

        // Parse children
        for (;;) {
            skip_ws();
            if (eof())
                break;
            if (peek() == '<') {
                if (p + 1 < end && p[1] == '/') {
                    // Close tag for current node
                    p += 2;
                    read_name(); // consume tag name
                    skip_ws();
                    if (peek() == '>')
                        ++p;
                    break;
                }
                XmlNode child;
                if (parse_node(child) && !child.tag.empty())
                    node.children.push_back(std::move(child));
            } else {
                // Text content - skip for SVG
                while (!eof() && peek() != '<')
                    ++p;
            }
        }
        return true;
    }
};

// ============================================================================
// Color parsing
// ============================================================================

static bool parse_hex_digit(char c, u8& out) {
    if (c >= '0' && c <= '9') {
        out = static_cast<u8>(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        out = static_cast<u8>(c - 'a' + 10);
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        out = static_cast<u8>(c - 'A' + 10);
        return true;
    }
    return false;
}

static Color parse_color(const char* s) {
    if (!s || !*s)
        return Color::black();

    // Named colors
    struct NamedColor {
        const char* name;
        u32 hex;
    };
    static const NamedColor named[] = {
        {"black", 0x000000},       {"white", 0xFFFFFF},     {"red", 0xFF0000},
        {"green", 0x008000},       {"blue", 0x0000FF},      {"yellow", 0xFFFF00},
        {"cyan", 0x00FFFF},        {"magenta", 0xFF00FF},   {"orange", 0xFFA500},
        {"purple", 0x800080},      {"pink", 0xFFC0CB},      {"brown", 0xA52A2A},
        {"gray", 0x808080},        {"grey", 0x808080},      {"silver", 0xC0C0C0},
        {"maroon", 0x800000},      {"olive", 0x808000},     {"lime", 0x00FF00},
        {"aqua", 0x00FFFF},        {"teal", 0x008080},      {"navy", 0x000080},
        {"fuchsia", 0xFF00FF},     {"gold", 0xFFD700},      {"coral", 0xFF7F50},
        {"tomato", 0xFF6347},      {"salmon", 0xFA8072},    {"crimson", 0xDC143C},
        {"darkred", 0x8B0000},     {"darkgreen", 0x006400}, {"darkblue", 0x00008B},
        {"lightgray", 0xD3D3D3},   {"lightgrey", 0xD3D3D3}, {"darkgray", 0xA9A9A9},
        {"darkgrey", 0xA9A9A9},    {"dimgray", 0x696969},   {"dimgrey", 0x696969},
        {"whitesmoke", 0xF5F5F5},  {"ivory", 0xFFFFF0},    {"beige", 0xF5F5DC},
        {"linen", 0xFAF0E6},       {"honeydew", 0xF0FFF0},
        {"indianred", 0xCD5C5C},   {"firebrick", 0xB22222},
        {"orangered", 0xFF4500},   {"darkorange", 0xFF8C00},
        {"khaki", 0xF0E68C},       {"darkkhaki", 0xBDB76B},
        {"plum", 0xDDA0DD},        {"violet", 0xEE82EE},
        {"orchid", 0xDA70D6},      {"mediumpurple", 0x9370DB},
        {"indigo", 0x4B0082},      {"slateblue", 0x6A5ACD},
        {"chartreuse", 0x7FFF00},  {"limegreen", 0x32CD32},
        {"springgreen", 0x00FF7F}, {"seagreen", 0x2E8B57},
        {"forestgreen", 0x228B22}, {"olivedrab", 0x6B8E23},
        {"darkcyan", 0x008B8B},    {"lightblue", 0xADD8E6},
        {"deepskyblue", 0x00BFFF}, {"dodgerblue", 0x1E90FF},
        {"royalblue", 0x4169E1},   {"steelblue", 0x4682B4},
        {"midnightblue", 0x191970},{"cornflowerblue", 0x6495ED},
        {"skyblue", 0x87CEEB},     {"turquoise", 0x40E0D0},
        {"chocolate", 0xD2691E},   {"sienna", 0xA0522D},
        {"peru", 0xCD853F},        {"tan", 0xD2B48C},
        {"rosybrown", 0xBC8F8F},   {"sandybrown", 0xF4A460},
        {"goldenrod", 0xDAA520},   {"wheat", 0xF5DEB3},
        {"lemonchiffon", 0xFFFACD},{"lightcoral", 0xF08080},
        {"lightsalmon", 0xFFA07A}, {"lightpink", 0xFFB6C1},
        {"hotpink", 0xFF69B4},     {"deeppink", 0xFF1493},
        {"mistyrose", 0xFFE4E1},   {"lavender", 0xE6E6FA},
        {"thistle", 0xD8BFD8},     {"snow", 0xFFFAFA},
        {"ghostwhite", 0xF8F8FF},  {"floralwhite", 0xFFFAF0},
        {"aliceblue", 0xF0F8FF},   {"antiquewhite", 0xFAEBD7},
        {"azure", 0xF0FFFF},       {"mintcream", 0xF5FFFA},
        {"oldlace", 0xFDF5E6},     {"papayawhip", 0xFFEFD5},
        {"blanchedalmond", 0xFFEBCD},{"bisque", 0xFFE4C4},
        {"moccasin", 0xFFE4B5},    {"navajowhite", 0xFFDEAD},
        {"peachpuff", 0xFFDAB9},   {"seashell", 0xFFF5EE},
        {"cornsilk", 0xFFF8DC},    {"lightyellow", 0xFFFFE0},
        {"lightgoldenrodyellow", 0xFAFAD2},
        {"palegreen", 0x98FB98},   {"lightgreen", 0x90EE90},
        {"mediumseagreen", 0x3CB371},{"darkseagreen", 0x8FBC8F},
        {"mediumaquamarine", 0x66CDAA},{"aquamarine", 0x7FFFD4},
        {"paleturquoise", 0xAFEEEE},{"lightcyan", 0xE0FFFF},
        {"mediumturquoise", 0x48D1CC},{"darkturquoise", 0x00CED1},
        {"cadetblue", 0x5F9EA0},   {"powderblue", 0xB0E0E6},
        {"lightsteelblue", 0xB0C4DE},{"slategray", 0x708090},
        {"slategrey", 0x708090},   {"lightslategray", 0x778899},
        {"lightslategrey", 0x778899},{"mediumslateblue", 0x7B68EE},
        {"mediumbluE", 0x0000CD},  {"darkviolet", 0x9400D3},
        {"darkorchid", 0x9932CC},  {"darkmagenta", 0x8B008B},
        {"blueviolet", 0x8A2BE2},  {"mediumvioletred", 0xC71585},
        {"palevioletred", 0xDB7093},{"mediumorchid", 0xBA55D3},
        {"mediumspringgreen", 0x00FA9A},{"yellowgreen", 0x9ACD32},
        {"darkolivegreen", 0x556B2F},{"greenyellow", 0xADFF2F},
        {"lawngreen", 0x7CFC00},   {"darksalmon", 0xE9967A},
    };

    for (auto& nc : named) {
        // Case-insensitive compare
        const char* a = s;
        const char* b = nc.name;
        bool match = true;
        while (*a && *b) {
            if (std::tolower(static_cast<u8>(*a)) != std::tolower(static_cast<u8>(*b))) {
                match = false;
                break;
            }
            ++a;
            ++b;
        }
        if (match && !*a && !*b)
            return Color::from_hex(nc.hex);
    }

    // #RGB, #RRGGBB, #RRGGBBAA
    if (s[0] == '#') {
        ++s;
        usize len = std::strlen(s);
        u8 digits[8] = {};
        for (usize i = 0; i < len && i < 8; ++i)
            parse_hex_digit(s[i], digits[i]);

        if (len == 3) {
            return Color::from_rgba8(static_cast<u8>(digits[0] * 17),
                                     static_cast<u8>(digits[1] * 17),
                                     static_cast<u8>(digits[2] * 17));
        }
        if (len == 4) {
            return Color::from_rgba8(static_cast<u8>(digits[0] * 17),
                                     static_cast<u8>(digits[1] * 17),
                                     static_cast<u8>(digits[2] * 17),
                                     static_cast<u8>(digits[3] * 17));
        }
        if (len == 6) {
            return Color::from_rgba8(static_cast<u8>(digits[0] * 16 + digits[1]),
                                     static_cast<u8>(digits[2] * 16 + digits[3]),
                                     static_cast<u8>(digits[4] * 16 + digits[5]));
        }
        if (len == 8) {
            return Color::from_rgba8(
                static_cast<u8>(digits[0] * 16 + digits[1]),
                static_cast<u8>(digits[2] * 16 + digits[3]),
                static_cast<u8>(digits[4] * 16 + digits[5]),
                static_cast<u8>(digits[6] * 16 + digits[7]));
        }
        return Color::black();
    }

    // rgb(r, g, b) or rgba(r, g, b, a)
    if (std::strncmp(s, "rgb", 3) == 0) {
        const char* p = s + 3;
        if (*p == 'a')
            ++p;
        if (*p == '(')
            ++p;
        f32 vals[4] = {0, 0, 0, 1};
        for (int i = 0; i < 4 && *p; ++i) {
            while (*p && (std::isspace(static_cast<u8>(*p)) || *p == ','))
                ++p;
            if (*p == ')')
                break;
            vals[i] = static_cast<f32>(std::atof(p));
            // Check for percentage
            while (*p && *p != ',' && *p != ')' && !std::isspace(static_cast<u8>(*p))) {
                if (*p == '%') {
                    vals[i] /= 100.0f;
                    if (i < 3)
                        vals[i] *= 255.0f;
                }
                ++p;
            }
        }
        return Color::from_rgba8(static_cast<u8>(clamp(vals[0], 0, 255)),
                                 static_cast<u8>(clamp(vals[1], 0, 255)),
                                 static_cast<u8>(clamp(vals[2], 0, 255)),
                                 static_cast<u8>(clamp(vals[3] * 255.0f, 0, 255)));
    }

    return Color::black();
}

// ============================================================================
// Paint parsing
// ============================================================================

static Paint parse_paint(const char* s) {
    Paint paint;
    if (!s || !*s || std::strcmp(s, "none") == 0 || std::strcmp(s, "transparent") == 0) {
        paint.type = Paint::None;
        return paint;
    }
    // url(#id) reference
    if (std::strncmp(s, "url(#", 5) == 0) {
        const char* start = s + 5;
        const char* end = std::strchr(start, ')');
        if (end) {
            paint.type = Paint::GradientRef;
            paint.gradient_id = std::string(start, end);
        }
        return paint;
    }
    paint.type = Paint::Solid;
    paint.color = parse_color(s);
    return paint;
}

// ============================================================================
// Number / coordinate parsing helpers
// ============================================================================

static const char* skip_ws_comma(const char* p) {
    while (*p && (std::isspace(static_cast<u8>(*p)) || *p == ','))
        ++p;
    return p;
}

static f32 parse_number(const char*& p) {
    p = skip_ws_comma(p);
    char* end;
    f32 val = std::strtof(p, &end);
    p = end;
    return val;
}

static f32 parse_coord_value(const char* s) {
    if (!s)
        return 0;
    char* end;
    f32 val = std::strtof(s, &end);
    // Skip units like px, pt, em, etc.
    return val;
}

// ============================================================================
// Transform parsing
// ============================================================================

static Transform parse_transform(const char* s) {
    if (!s || !*s)
        return Transform::identity();

    Transform result = Transform::identity();
    const char* p = s;

    while (*p) {
        while (*p && std::isspace(static_cast<u8>(*p)))
            ++p;
        if (!*p)
            break;

        Transform t = Transform::identity();

        if (std::strncmp(p, "matrix", 6) == 0) {
            p += 6;
            while (*p && *p != '(')
                ++p;
            if (*p)
                ++p;
            t.a = parse_number(p);
            t.b = parse_number(p);
            t.c = parse_number(p);
            t.d = parse_number(p);
            t.e = parse_number(p);
            t.f = parse_number(p);
            while (*p && *p != ')')
                ++p;
            if (*p)
                ++p;
        } else if (std::strncmp(p, "translate", 9) == 0) {
            p += 9;
            while (*p && *p != '(')
                ++p;
            if (*p)
                ++p;
            f32 tx = parse_number(p);
            p = skip_ws_comma(p);
            f32 ty = (*p && *p != ')') ? parse_number(p) : 0;
            t = Transform::translate(tx, ty);
            while (*p && *p != ')')
                ++p;
            if (*p)
                ++p;
        } else if (std::strncmp(p, "scale", 5) == 0) {
            p += 5;
            while (*p && *p != '(')
                ++p;
            if (*p)
                ++p;
            f32 sx = parse_number(p);
            p = skip_ws_comma(p);
            f32 sy = (*p && *p != ')') ? parse_number(p) : sx;
            t = Transform::scale(sx, sy);
            while (*p && *p != ')')
                ++p;
            if (*p)
                ++p;
        } else if (std::strncmp(p, "rotate", 6) == 0) {
            p += 6;
            while (*p && *p != '(')
                ++p;
            if (*p)
                ++p;
            f32 angle = parse_number(p);
            p = skip_ws_comma(p);
            if (*p && *p != ')') {
                f32 cx = parse_number(p);
                f32 cy = parse_number(p);
                t = Transform::translate(cx, cy) * Transform::rotate(angle) *
                    Transform::translate(-cx, -cy);
            } else {
                t = Transform::rotate(angle);
            }
            while (*p && *p != ')')
                ++p;
            if (*p)
                ++p;
        } else if (std::strncmp(p, "skewX", 5) == 0) {
            p += 5;
            while (*p && *p != '(')
                ++p;
            if (*p)
                ++p;
            f32 angle = parse_number(p);
            f32 rad = angle * 3.14159265358979323846f / 180.0f;
            t.c = std::tan(rad);
            while (*p && *p != ')')
                ++p;
            if (*p)
                ++p;
        } else if (std::strncmp(p, "skewY", 5) == 0) {
            p += 5;
            while (*p && *p != '(')
                ++p;
            if (*p)
                ++p;
            f32 angle = parse_number(p);
            f32 rad = angle * 3.14159265358979323846f / 180.0f;
            t.b = std::tan(rad);
            while (*p && *p != ')')
                ++p;
            if (*p)
                ++p;
        } else {
            ++p; // skip unknown
            continue;
        }

        result = result * t;
    }
    return result;
}

// ============================================================================
// SVG path data parsing (d="...")
// ============================================================================

static constexpr f32 PI = 3.14159265358979323846f;

// Convert SVG arc endpoint parameterization to center + cubic beziers
static void arc_to_cubics(Path& path, Vec2 from, f32 rx, f32 ry, f32 x_rotation, bool large_arc,
                          bool sweep, Vec2 to) {
    if (rx == 0 || ry == 0) {
        path.line_to(to);
        return;
    }

    f32 phi = x_rotation * PI / 180.0f;
    f32 cos_phi = std::cos(phi);
    f32 sin_phi = std::sin(phi);

    f32 dx = (from.x - to.x) * 0.5f;
    f32 dy = (from.y - to.y) * 0.5f;
    f32 x1p = cos_phi * dx + sin_phi * dy;
    f32 y1p = -sin_phi * dx + cos_phi * dy;

    rx = std::fabs(rx);
    ry = std::fabs(ry);

    // Correct radii
    f32 lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1.0f) {
        f32 s = std::sqrt(lambda);
        rx *= s;
        ry *= s;
    }

    f32 rx2 = rx * rx, ry2 = ry * ry;
    f32 x1p2 = x1p * x1p, y1p2 = y1p * y1p;

    f32 num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
    f32 den = rx2 * y1p2 + ry2 * x1p2;
    f32 sq = (den > 0) ? std::sqrt(std::fmax(0.0f, num / den)) : 0;
    if (large_arc == sweep)
        sq = -sq;

    f32 cxp = sq * rx * y1p / ry;
    f32 cyp = sq * -ry * x1p / rx;

    f32 cx = cos_phi * cxp - sin_phi * cyp + (from.x + to.x) * 0.5f;
    f32 cy = sin_phi * cxp + cos_phi * cyp + (from.y + to.y) * 0.5f;

    auto angle_between = [](f32 ux, f32 uy, f32 vx, f32 vy) -> f32 {
        f32 n = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        if (n == 0)
            return 0;
        f32 c = (ux * vx + uy * vy) / n;
        c = clamp(c, -1.0f, 1.0f);
        f32 a = std::acos(c);
        if (ux * vy - uy * vx < 0)
            a = -a;
        return a;
    };

    f32 theta1 = angle_between(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    f32 dtheta = angle_between((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx,
                               (-y1p - cyp) / ry);

    if (!sweep && dtheta > 0)
        dtheta -= 2 * PI;
    if (sweep && dtheta < 0)
        dtheta += 2 * PI;

    // Split into segments of at most PI/2
    i32 segments = static_cast<i32>(std::ceil(std::fabs(dtheta) / (PI * 0.5f)));
    if (segments < 1)
        segments = 1;
    f32 seg_angle = dtheta / segments;
    f32 alpha = 4.0f / 3.0f * std::tan(seg_angle * 0.25f);

    f32 cur_angle = theta1;
    for (i32 i = 0; i < segments; ++i) {
        f32 a0 = cur_angle;
        f32 a1 = cur_angle + seg_angle;
        f32 cos0 = std::cos(a0), sin0 = std::sin(a0);
        f32 cos1 = std::cos(a1), sin1 = std::sin(a1);

        // Unit circle control points
        f32 ep1x = cos0 - alpha * sin0;
        f32 ep1y = sin0 + alpha * cos0;
        f32 ep2x = cos1 + alpha * sin1;
        f32 ep2y = sin1 - alpha * cos1;
        f32 ep3x = cos1;
        f32 ep3y = sin1;

        // Scale and rotate to ellipse
        auto to_world = [&](f32 ex, f32 ey) -> Vec2 {
            f32 x = ex * rx;
            f32 y = ey * ry;
            return {cos_phi * x - sin_phi * y + cx, sin_phi * x + cos_phi * y + cy};
        };

        path.cubic_to(to_world(ep1x, ep1y), to_world(ep2x, ep2y), to_world(ep3x, ep3y));
        cur_angle = a1;
    }
}

void parse_path_data(const char* d, Path& path) {
    if (!d)
        return;

    const char* p = d;
    char cmd = 0;
    Vec2 cur = {0, 0};
    Vec2 start = {0, 0};      // start of current subpath
    Vec2 last_ctrl = {0, 0};  // last control point (for S/T)

    auto next_num = [&]() -> f32 { return parse_number(p); };

    auto next_flag = [&]() -> bool {
        p = skip_ws_comma(p);
        bool val = (*p == '1');
        if (*p)
            ++p;
        return val;
    };

    while (*p) {
        p = skip_ws_comma(p);
        if (!*p)
            break;

        // Check for command letter
        if (std::isalpha(static_cast<u8>(*p))) {
            cmd = *p++;
        }

        bool relative = (cmd >= 'a' && cmd <= 'z');
        Vec2 base = relative ? cur : Vec2{0, 0};

        switch (cmd | 0x20) { // to lowercase
        case 'm': {
            f32 x = next_num() + base.x;
            f32 y = next_num() + base.y;
            cur = {x, y};
            start = cur;
            path.move_to(cur);
            // Subsequent coordinates are implicit LineTo
            cmd = relative ? 'l' : 'L';
            break;
        }
        case 'l': {
            f32 x = next_num() + base.x;
            f32 y = next_num() + base.y;
            cur = {x, y};
            path.line_to(cur);
            break;
        }
        case 'h': {
            f32 x = next_num() + (relative ? cur.x : 0);
            cur.x = x;
            path.line_to(cur);
            break;
        }
        case 'v': {
            f32 y = next_num() + (relative ? cur.y : 0);
            cur.y = y;
            path.line_to(cur);
            break;
        }
        case 'c': {
            f32 x1 = next_num() + base.x, y1 = next_num() + base.y;
            f32 x2 = next_num() + base.x, y2 = next_num() + base.y;
            f32 x = next_num() + base.x, y = next_num() + base.y;
            path.cubic_to({x1, y1}, {x2, y2}, {x, y});
            last_ctrl = {x2, y2};
            cur = {x, y};
            break;
        }
        case 's': {
            // Reflect previous control point
            Vec2 c1 = {2 * cur.x - last_ctrl.x, 2 * cur.y - last_ctrl.y};
            f32 x2 = next_num() + base.x, y2 = next_num() + base.y;
            f32 x = next_num() + base.x, y = next_num() + base.y;
            path.cubic_to(c1, {x2, y2}, {x, y});
            last_ctrl = {x2, y2};
            cur = {x, y};
            break;
        }
        case 'q': {
            f32 x1 = next_num() + base.x, y1 = next_num() + base.y;
            f32 x = next_num() + base.x, y = next_num() + base.y;
            // Convert quadratic to cubic
            Vec2 c1 = {cur.x + 2.0f / 3.0f * (x1 - cur.x), cur.y + 2.0f / 3.0f * (y1 - cur.y)};
            Vec2 c2 = {x + 2.0f / 3.0f * (x1 - x), y + 2.0f / 3.0f * (y1 - y)};
            path.cubic_to(c1, c2, {x, y});
            last_ctrl = {x1, y1};
            cur = {x, y};
            break;
        }
        case 't': {
            Vec2 ctrl = {2 * cur.x - last_ctrl.x, 2 * cur.y - last_ctrl.y};
            f32 x = next_num() + base.x, y = next_num() + base.y;
            Vec2 c1 = {cur.x + 2.0f / 3.0f * (ctrl.x - cur.x),
                       cur.y + 2.0f / 3.0f * (ctrl.y - cur.y)};
            Vec2 c2 = {x + 2.0f / 3.0f * (ctrl.x - x), y + 2.0f / 3.0f * (ctrl.y - y)};
            path.cubic_to(c1, c2, {x, y});
            last_ctrl = ctrl;
            cur = {x, y};
            break;
        }
        case 'a': {
            f32 rx = next_num(), ry = next_num();
            f32 x_rot = next_num();
            bool large = next_flag();
            bool sweep = next_flag();
            f32 x = next_num() + base.x, y = next_num() + base.y;
            arc_to_cubics(path, cur, rx, ry, x_rot, large, sweep, {x, y});
            cur = {x, y};
            last_ctrl = cur;
            break;
        }
        case 'z': {
            path.close();
            cur = start;
            last_ctrl = cur;
            break;
        }
        default:
            ++p; // skip unknown
            break;
        }
    }
}

// ============================================================================
// SVG basic shape -> path conversion
// ============================================================================

static Path rect_to_path(f32 x, f32 y, f32 w, f32 h, f32 rx, f32 ry) {
    Path path;
    if (rx <= 0 && ry <= 0) {
        path.move_to({x, y});
        path.line_to({x + w, y});
        path.line_to({x + w, y + h});
        path.line_to({x, y + h});
        path.close();
    } else {
        if (rx > w * 0.5f)
            rx = w * 0.5f;
        if (ry > h * 0.5f)
            ry = h * 0.5f;
        if (rx > 0 && ry <= 0)
            ry = rx;
        if (ry > 0 && rx <= 0)
            rx = ry;
        // Kappa for 90-degree circular arc approximation
        f32 k = 0.5522847498f;
        f32 kx = rx * k, ky = ry * k;

        path.move_to({x + rx, y});
        path.line_to({x + w - rx, y});
        path.cubic_to({x + w - rx + kx, y}, {x + w, y + ry - ky}, {x + w, y + ry});
        path.line_to({x + w, y + h - ry});
        path.cubic_to({x + w, y + h - ry + ky}, {x + w - rx + kx, y + h}, {x + w - rx, y + h});
        path.line_to({x + rx, y + h});
        path.cubic_to({x + rx - kx, y + h}, {x, y + h - ry + ky}, {x, y + h - ry});
        path.line_to({x, y + ry});
        path.cubic_to({x, y + ry - ky}, {x + rx - kx, y}, {x + rx, y});
        path.close();
    }
    return path;
}

static Path circle_to_path(f32 cx, f32 cy, f32 r) {
    Path path;
    f32 k = r * 0.5522847498f;
    path.move_to({cx + r, cy});
    path.cubic_to({cx + r, cy + k}, {cx + k, cy + r}, {cx, cy + r});
    path.cubic_to({cx - k, cy + r}, {cx - r, cy + k}, {cx - r, cy});
    path.cubic_to({cx - r, cy - k}, {cx - k, cy - r}, {cx, cy - r});
    path.cubic_to({cx + k, cy - r}, {cx + r, cy - k}, {cx + r, cy});
    path.close();
    return path;
}

static Path ellipse_to_path(f32 cx, f32 cy, f32 rx, f32 ry) {
    Path path;
    f32 kx = rx * 0.5522847498f;
    f32 ky = ry * 0.5522847498f;
    path.move_to({cx + rx, cy});
    path.cubic_to({cx + rx, cy + ky}, {cx + kx, cy + ry}, {cx, cy + ry});
    path.cubic_to({cx - kx, cy + ry}, {cx - rx, cy + ky}, {cx - rx, cy});
    path.cubic_to({cx - rx, cy - ky}, {cx - kx, cy - ry}, {cx, cy - ry});
    path.cubic_to({cx + kx, cy - ry}, {cx + rx, cy - ky}, {cx + rx, cy});
    path.close();
    return path;
}

static Path line_to_path(f32 x1, f32 y1, f32 x2, f32 y2) {
    Path path;
    path.move_to({x1, y1});
    path.line_to({x2, y2});
    return path;
}

static Path polyline_to_path(const char* points_str, bool close) {
    Path path;
    if (!points_str)
        return path;

    const char* p = points_str;
    bool first = true;
    while (*p) {
        p = skip_ws_comma(p);
        if (!*p)
            break;
        f32 x = parse_number(p);
        f32 y = parse_number(p);
        if (first) {
            path.move_to({x, y});
            first = false;
        } else {
            path.line_to({x, y});
        }
    }
    if (close && !first)
        path.close();
    return path;
}

// ============================================================================
// Style attribute parsing ("fill:red;stroke:blue;stroke-width:2")
// ============================================================================

struct StyleAttrs {
    const char* fill = nullptr;
    const char* stroke = nullptr;
    const char* stroke_width = nullptr;
    const char* opacity = nullptr;
    const char* fill_opacity = nullptr;
    const char* stroke_opacity = nullptr;
    const char* fill_rule = nullptr;
    const char* transform = nullptr;

    // Temp storage for parsed style attribute values
    std::vector<std::string> storage;
};

static void parse_style_attr(const char* style_str, StyleAttrs& out) {
    if (!style_str)
        return;
    out.storage.reserve(8); // prevent reallocation invalidating c_str() pointers
    const char* p = style_str;
    while (*p) {
        while (*p && std::isspace(static_cast<u8>(*p)))
            ++p;
        const char* key_start = p;
        while (*p && *p != ':' && *p != ';')
            ++p;
        if (*p != ':') {
            if (*p)
                ++p;
            continue;
        }
        std::string key(key_start, p);
        ++p; // skip ':'
        while (*p && std::isspace(static_cast<u8>(*p)))
            ++p;
        const char* val_start = p;
        while (*p && *p != ';')
            ++p;
        // Trim trailing whitespace
        const char* val_end = p;
        while (val_end > val_start && std::isspace(static_cast<u8>(val_end[-1])))
            --val_end;
        out.storage.emplace_back(val_start, val_end);
        const char* val = out.storage.back().c_str();

        if (key == "fill")
            out.fill = val;
        else if (key == "stroke")
            out.stroke = val;
        else if (key == "stroke-width")
            out.stroke_width = val;
        else if (key == "opacity")
            out.opacity = val;
        else if (key == "fill-opacity")
            out.fill_opacity = val;
        else if (key == "stroke-opacity")
            out.stroke_opacity = val;
        else if (key == "fill-rule")
            out.fill_rule = val;
        else if (key == "transform")
            out.transform = val;

        if (*p)
            ++p; // skip ';'
    }
}

// ============================================================================
// Gradient parsing
// ============================================================================

static void parse_gradient_stops(const XmlNode& node, Gradient& grad) {
    for (auto& child : node.children) {
        if (child.tag != "stop")
            continue;
        GradientStop stop;
        const char* offset_str = child.attr("offset");
        if (offset_str) {
            stop.offset = static_cast<f32>(std::atof(offset_str));
            if (std::strchr(offset_str, '%'))
                stop.offset /= 100.0f;
        }
        const char* color_str = child.attr("stop-color");
        stop.color = color_str ? parse_color(color_str) : Color::black();

        const char* opacity_str = child.attr("stop-opacity");
        if (opacity_str)
            stop.color = stop.color.with_alpha(static_cast<f32>(std::atof(opacity_str)));

        // Check style attribute for stop-color/stop-opacity
        StyleAttrs sa;
        parse_style_attr(child.attr("style"), sa);
        // (style can override attributes but we keep it simple)

        grad.stops.push_back(stop);
    }
    std::sort(grad.stops.begin(), grad.stops.end(),
              [](const GradientStop& a, const GradientStop& b) { return a.offset < b.offset; });
}

static void parse_linear_gradient(const XmlNode& node, Document& doc) {
    Gradient grad;
    grad.type = GradientType::Linear;
    const char* id = node.attr("id");
    if (!id)
        return;

    grad.x1 = node.attr_f("x1", 0);
    grad.y1 = node.attr_f("y1", 0);
    grad.x2 = node.attr_f("x2", 1);
    grad.y2 = node.attr_f("y2", 0);

    // Check for percentage values (default for objectBoundingBox)
    auto is_percent = [](const char* s) -> bool {
        return s && std::strchr(s, '%') != nullptr;
    };
    if (is_percent(node.attr("x1")) || is_percent(node.attr("y1")) ||
        is_percent(node.attr("x2")) || is_percent(node.attr("y2"))) {
        grad.x1 /= 100.0f;
        grad.y1 /= 100.0f;
        grad.x2 /= 100.0f;
        grad.y2 /= 100.0f;
    }

    const char* units = node.attr("gradientUnits");
    grad.user_space = units && std::strcmp(units, "userSpaceOnUse") == 0;

    const char* xform = node.attr("gradientTransform");
    if (xform)
        grad.transform = parse_transform(xform);

    const char* spread = node.attr("spreadMethod");
    if (spread) {
        if (std::strcmp(spread, "reflect") == 0)
            grad.spread = SpreadMethod::Reflect;
        else if (std::strcmp(spread, "repeat") == 0)
            grad.spread = SpreadMethod::Repeat;
    }

    // Inherit from href/xlink:href
    const char* href = node.attr("href");
    if (!href)
        href = node.attr("xlink:href");
    if (href && href[0] == '#') {
        auto it = doc.gradients.find(href + 1);
        if (it != doc.gradients.end() && grad.stops.empty())
            grad.stops = it->second.stops;
    }

    parse_gradient_stops(node, grad);
    doc.gradients[id] = std::move(grad);
}

static void parse_radial_gradient(const XmlNode& node, Document& doc) {
    Gradient grad;
    grad.type = GradientType::Radial;
    const char* id = node.attr("id");
    if (!id)
        return;

    grad.cx = node.attr_f("cx", 0.5f);
    grad.cy = node.attr_f("cy", 0.5f);
    grad.r = node.attr_f("r", 0.5f);
    grad.fx = node.attr_f("fx", -1);
    grad.fy = node.attr_f("fy", -1);

    const char* units = node.attr("gradientUnits");
    grad.user_space = units && std::strcmp(units, "userSpaceOnUse") == 0;

    const char* xform = node.attr("gradientTransform");
    if (xform)
        grad.transform = parse_transform(xform);

    const char* spread = node.attr("spreadMethod");
    if (spread) {
        if (std::strcmp(spread, "reflect") == 0)
            grad.spread = SpreadMethod::Reflect;
        else if (std::strcmp(spread, "repeat") == 0)
            grad.spread = SpreadMethod::Repeat;
    }

    const char* href = node.attr("href");
    if (!href)
        href = node.attr("xlink:href");
    if (href && href[0] == '#') {
        auto it = doc.gradients.find(href + 1);
        if (it != doc.gradients.end() && grad.stops.empty())
            grad.stops = it->second.stops;
    }

    parse_gradient_stops(node, grad);
    doc.gradients[id] = std::move(grad);
}

// ============================================================================
// SVG element -> Shape
// ============================================================================

struct ParseCtx {
    Transform transform = Transform::identity();
    Paint fill = {Paint::Solid, Color::black(), {}};
    Paint stroke = {Paint::None, Color::black(), {}};
    f32 stroke_width = 1.0f;
    f32 opacity = 1.0f;
    f32 fill_opacity = 1.0f;
    f32 stroke_opacity = 1.0f;
    FillRule fill_rule = FillRule::NonZero;
};

static void apply_attrs(const XmlNode& node, ParseCtx& ctx) {
    // First apply style= attribute (can contain any of the below)
    StyleAttrs sa;
    parse_style_attr(node.attr("style"), sa);

    // Direct attributes (style= overrides these)
    auto resolve = [&](const char* attr_name, const char* style_val) -> const char* {
        return style_val ? style_val : node.attr(attr_name);
    };

    const char* fill_str = resolve("fill", sa.fill);
    if (fill_str)
        ctx.fill = parse_paint(fill_str);

    const char* stroke_str = resolve("stroke", sa.stroke);
    if (stroke_str)
        ctx.stroke = parse_paint(stroke_str);

    const char* sw = resolve("stroke-width", sa.stroke_width);
    if (sw)
        ctx.stroke_width = static_cast<f32>(std::atof(sw));

    const char* op = resolve("opacity", sa.opacity);
    if (op)
        ctx.opacity = static_cast<f32>(std::atof(op));

    const char* fo = resolve("fill-opacity", sa.fill_opacity);
    if (fo)
        ctx.fill_opacity = static_cast<f32>(std::atof(fo));

    const char* so = resolve("stroke-opacity", sa.stroke_opacity);
    if (so)
        ctx.stroke_opacity = static_cast<f32>(std::atof(so));

    const char* fr = resolve("fill-rule", sa.fill_rule);
    if (fr) {
        if (std::strcmp(fr, "evenodd") == 0)
            ctx.fill_rule = FillRule::EvenOdd;
        else
            ctx.fill_rule = FillRule::NonZero;
    }

    const char* xform = resolve("transform", sa.transform);
    if (xform)
        ctx.transform = ctx.transform * parse_transform(xform);
}

static void add_shape(Document& doc, Path&& path, const ParseCtx& ctx) {
    Shape shape;
    shape.path = std::move(path);
    shape.fill = ctx.fill;
    shape.stroke = ctx.stroke;
    shape.stroke_width = ctx.stroke_width;
    shape.opacity = ctx.opacity;
    shape.fill_opacity = ctx.fill_opacity;
    shape.stroke_opacity = ctx.stroke_opacity;
    shape.fill_rule = ctx.fill_rule;
    shape.transform = ctx.transform;
    doc.shapes.push_back(std::move(shape));
}

static void parse_element(const XmlNode& node, Document& doc, ParseCtx parent_ctx) {
    ParseCtx ctx = parent_ctx;
    apply_attrs(node, ctx);

    if (node.tag == "rect") {
        f32 x = node.attr_f("x"), y = node.attr_f("y");
        f32 w = node.attr_f("width"), h = node.attr_f("height");
        f32 rx = node.attr_f("rx"), ry = node.attr_f("ry");
        if (w > 0 && h > 0)
            add_shape(doc, rect_to_path(x, y, w, h, rx, ry), ctx);
    } else if (node.tag == "circle") {
        f32 cx = node.attr_f("cx"), cy = node.attr_f("cy"), r = node.attr_f("r");
        if (r > 0)
            add_shape(doc, circle_to_path(cx, cy, r), ctx);
    } else if (node.tag == "ellipse") {
        f32 cx = node.attr_f("cx"), cy = node.attr_f("cy");
        f32 rx = node.attr_f("rx"), ry = node.attr_f("ry");
        if (rx > 0 && ry > 0)
            add_shape(doc, ellipse_to_path(cx, cy, rx, ry), ctx);
    } else if (node.tag == "line") {
        f32 x1 = node.attr_f("x1"), y1 = node.attr_f("y1");
        f32 x2 = node.attr_f("x2"), y2 = node.attr_f("y2");
        add_shape(doc, line_to_path(x1, y1, x2, y2), ctx);
    } else if (node.tag == "polyline") {
        add_shape(doc, polyline_to_path(node.attr("points"), false), ctx);
    } else if (node.tag == "polygon") {
        add_shape(doc, polyline_to_path(node.attr("points"), true), ctx);
    } else if (node.tag == "path") {
        Path path;
        parse_path_data(node.attr("d"), path);
        if (!path.entries.empty())
            add_shape(doc, std::move(path), ctx);
    } else if (node.tag == "g" || node.tag == "svg" || node.tag == "symbol" || node.tag == "use") {
        // Group - recurse with inherited context
        for (auto& child : node.children)
            parse_element(child, doc, ctx);
        return;
    } else if (node.tag == "defs") {
        // Parse gradient definitions
        for (auto& child : node.children) {
            if (child.tag == "linearGradient")
                parse_linear_gradient(child, doc);
            else if (child.tag == "radialGradient")
                parse_radial_gradient(child, doc);
            // Nested defs or groups in defs
            else if (child.tag == "g") {
                for (auto& gc : child.children) {
                    if (gc.tag == "linearGradient")
                        parse_linear_gradient(gc, doc);
                    else if (gc.tag == "radialGradient")
                        parse_radial_gradient(gc, doc);
                }
            }
        }
        return;
    } else if (node.tag == "linearGradient") {
        parse_linear_gradient(node, doc);
        return;
    } else if (node.tag == "radialGradient") {
        parse_radial_gradient(node, doc);
        return;
    }

    // Recurse children for elements that may contain shapes
    for (auto& child : node.children)
        parse_element(child, doc, ctx);
}

// ============================================================================
// Top-level SVG parser
// ============================================================================

bool parse_svg(const char* data, usize length, Document& out) {
    XmlParser xml;
    xml.p = data;
    xml.end = data + length;

    XmlNode root;
    // Find the <svg> root
    while (!xml.eof()) {
        XmlNode node;
        if (xml.parse_node(node)) {
            if (node.tag == "svg") {
                root = std::move(node);
                break;
            }
        }
    }

    if (root.tag != "svg")
        return false;

    // Parse dimensions
    const char* width_str = root.attr("width");
    const char* height_str = root.attr("height");
    const char* viewbox = root.attr("viewBox");

    if (viewbox) {
        const char* p = viewbox;
        out.view_x = parse_number(p);
        out.view_y = parse_number(p);
        out.view_w = parse_number(p);
        out.view_h = parse_number(p);
    }

    if (width_str)
        out.width = parse_coord_value(width_str);
    if (height_str)
        out.height = parse_coord_value(height_str);

    // Fall back to viewBox dimensions
    if (out.width <= 0 && out.view_w > 0)
        out.width = out.view_w;
    if (out.height <= 0 && out.view_h > 0)
        out.height = out.view_h;

    // Default if nothing specified
    if (out.width <= 0)
        out.width = 300;
    if (out.height <= 0)
        out.height = 150;

    // Parse all children
    ParseCtx ctx;
    apply_attrs(root, ctx);

    for (auto& child : root.children)
        parse_element(child, out, ctx);

    return true;
}

} // namespace svg
} // namespace ugui
