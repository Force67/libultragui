#include <ultragui/svg/svg.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                                                 \
    static void test_##name();                                                                     \
    static struct Register_##name {                                                                \
        Register_##name() {                                                                        \
            ++tests_run;                                                                           \
            std::printf("  %-50s", #name "...");                                                   \
            test_##name();                                                                         \
            std::printf(" PASS\n");                                                                \
            ++tests_passed;                                                                        \
        }                                                                                          \
    } reg_##name;                                                                                  \
    static void test_##name()

#define ASSERT(cond)                                                                               \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::printf(" FAIL\n    Assertion failed: %s\n    at %s:%d\n", #cond, __FILE__,        \
                        __LINE__);                                                                 \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)

// Helper: check if a pixel at (x,y) has non-zero alpha
static bool has_content(const ugui::SvgImage& img, ugui::u32 x, ugui::u32 y) {
    if (x >= img.width || y >= img.height)
        return false;
    return img.pixels[(y * img.width + x) * 4 + 3] > 0;
}

// Helper: get pixel color
static void get_pixel(const ugui::SvgImage& img, ugui::u32 x, ugui::u32 y, ugui::u8& r,
                      ugui::u8& g, ugui::u8& b, ugui::u8& a) {
    ugui::u32 idx = (y * img.width + x) * 4;
    r = img.pixels[idx + 0];
    g = img.pixels[idx + 1];
    b = img.pixels[idx + 2];
    a = img.pixels[idx + 3];
}

// ============================================================================
// Tests
// ============================================================================

TEST(parse_simple_rect) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <rect x="10" y="10" width="80" height="80" fill="red"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    ASSERT(img.width == 100);
    ASSERT(img.height == 100);
    ASSERT(img.pixels.size() == 100 * 100 * 4);

    // Center should be filled (red)
    ASSERT(has_content(img, 50, 50));
    // Corner should be empty
    ASSERT(!has_content(img, 0, 0));
}

TEST(parse_circle) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <circle cx="50" cy="50" r="40" fill="blue"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    // Center should be filled
    ASSERT(has_content(img, 50, 50));
    // Far corner should be empty
    ASSERT(!has_content(img, 0, 0));
    ASSERT(!has_content(img, 99, 0));

    // Check it's actually blue
    ugui::u8 r, g, b, a;
    get_pixel(img, 50, 50, r, g, b, a);
    ASSERT(r == 0);
    ASSERT(g == 0);
    ASSERT(b == 255);
    ASSERT(a == 255);
}

TEST(parse_ellipse) {
    const char* svg = R"(
        <svg width="200" height="100" xmlns="http://www.w3.org/2000/svg">
            <ellipse cx="100" cy="50" rx="80" ry="30" fill="green"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    ASSERT(img.width == 200);
    ASSERT(img.height == 100);
    ASSERT(has_content(img, 100, 50)); // center
    ASSERT(!has_content(img, 0, 0));   // corner
}

TEST(parse_polygon) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <polygon points="50,10 90,90 10,90" fill="yellow"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    // Center of triangle should be filled
    ASSERT(has_content(img, 50, 60));
    // Top-left corner should be empty
    ASSERT(!has_content(img, 5, 5));
}

TEST(parse_path_moveto_lineto) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <path d="M10,10 L90,10 L90,90 L10,90 Z" fill="#ff8800"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    ASSERT(has_content(img, 50, 50));
    ASSERT(!has_content(img, 0, 0));
}

TEST(parse_path_cubic_bezier) {
    const char* svg = R"(
        <svg width="200" height="200" xmlns="http://www.w3.org/2000/svg">
            <path d="M10,100 C10,10 190,10 190,100 L190,190 L10,190 Z" fill="purple"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    // Bottom center should be filled
    ASSERT(has_content(img, 100, 150));
}

TEST(parse_path_arc) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <path d="M50,10 A40,40 0 1,1 50,90 A40,40 0 1,1 50,10 Z" fill="cyan"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    ASSERT(has_content(img, 50, 50));
}

TEST(viewbox_scaling) {
    const char* svg = R"(
        <svg viewBox="0 0 10 10" xmlns="http://www.w3.org/2000/svg">
            <rect x="2" y="2" width="6" height="6" fill="white"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img, 100, 100);
    ASSERT(ok);
    ASSERT(img.width == 100);
    ASSERT(img.height == 100);
    // Rect at 2,2 size 6x6 in viewBox 0-10 -> should be at 20,20 size 60x60 in 100x100
    ASSERT(has_content(img, 50, 50));
    ASSERT(!has_content(img, 5, 5));
}

TEST(hex_colors) {
    const char* svg = R"(
        <svg width="30" height="10" xmlns="http://www.w3.org/2000/svg">
            <rect x="0"  y="0" width="10" height="10" fill="#ff0000"/>
            <rect x="10" y="0" width="10" height="10" fill="#00ff00"/>
            <rect x="20" y="0" width="10" height="10" fill="#0000ff"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);

    ugui::u8 r, g, b, a;
    get_pixel(img, 5, 5, r, g, b, a);
    ASSERT(r == 255 && g == 0 && b == 0);

    get_pixel(img, 15, 5, r, g, b, a);
    ASSERT(r == 0 && g == 255 && b == 0);

    get_pixel(img, 25, 5, r, g, b, a);
    ASSERT(r == 0 && g == 0 && b == 255);
}

TEST(opacity) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <rect x="0" y="0" width="100" height="100" fill="white" opacity="0.5"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);

    ugui::u8 r, g, b, a;
    get_pixel(img, 50, 50, r, g, b, a);
    // White at 50% opacity -> alpha should be ~128
    ASSERT(a > 100 && a < 155);
}

TEST(stroke_only) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <rect x="20" y="20" width="60" height="60" fill="none" stroke="red" stroke-width="4"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    // Center should be empty (fill=none)
    ASSERT(!has_content(img, 50, 50));
    // Border area should have content
    ASSERT(has_content(img, 20, 20));
}

TEST(group_transform) {
    const char* svg = R"SVG(
        <svg width="200" height="200" xmlns="http://www.w3.org/2000/svg">
            <g transform="translate(100,100)">
                <rect x="-20" y="-20" width="40" height="40" fill="orange"/>
            </g>
        </svg>
    )SVG";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    // Rect should be centered at (100,100)
    ASSERT(has_content(img, 100, 100));
    ASSERT(!has_content(img, 10, 10));
}

TEST(linear_gradient) {
    const char* svg = R"SVG(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <defs>
                <linearGradient id="grad1" x1="0" y1="0" x2="1" y2="0">
                    <stop offset="0" stop-color="red"/>
                    <stop offset="1" stop-color="blue"/>
                </linearGradient>
            </defs>
            <rect x="0" y="0" width="100" height="100" fill="url(#grad1)"/>
        </svg>
    )SVG";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);

    // Left side should be reddish
    ugui::u8 r, g, b, a;
    get_pixel(img, 5, 50, r, g, b, a);
    ASSERT(r > 200);
    ASSERT(b < 50);

    // Right side should be bluish
    get_pixel(img, 95, 50, r, g, b, a);
    ASSERT(b > 200);
    ASSERT(r < 50);
}

TEST(style_attribute) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <rect x="10" y="10" width="80" height="80" style="fill:green;opacity:1"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);

    ugui::u8 r, g, b, a;
    get_pixel(img, 50, 50, r, g, b, a);
    ASSERT(r == 0);
    ASSERT(g == 128); // CSS "green" is #008000
    ASSERT(b == 0);
}

TEST(multiple_shapes_layered) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <rect x="0" y="0" width="100" height="100" fill="red"/>
            <circle cx="50" cy="50" r="30" fill="blue"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);

    // Center should be blue (circle on top)
    ugui::u8 r, g, b, a;
    get_pixel(img, 50, 50, r, g, b, a);
    ASSERT(r == 0 && b == 255);

    // Corner should be red (rect behind)
    get_pixel(img, 5, 5, r, g, b, a);
    ASSERT(r == 255 && b == 0);
}

TEST(rounded_rect) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <rect x="10" y="10" width="80" height="80" rx="15" ry="15" fill="white"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    // Center should be filled
    ASSERT(has_content(img, 50, 50));
    // Extreme corner of the rect should be empty (rounded off)
    ASSERT(!has_content(img, 11, 11));
}

TEST(target_size_override) {
    const char* svg = R"(
        <svg width="50" height="50" xmlns="http://www.w3.org/2000/svg">
            <rect x="0" y="0" width="50" height="50" fill="white"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img, 200, 200);
    ASSERT(ok);
    ASSERT(img.width == 200);
    ASSERT(img.height == 200);
    // Should still be filled at the larger size
    ASSERT(has_content(img, 100, 100));
}

TEST(empty_svg_fails) {
    const char* svg = "";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, 0, img);
    ASSERT(!ok);
}

TEST(invalid_svg_fails) {
    const char* svg = "<html><body>Not SVG</body></html>";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(!ok);
}

TEST(polyline_stroke) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <polyline points="10,10 90,10 90,90" fill="none" stroke="white" stroke-width="4"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    // Top edge should have stroke content
    ASSERT(has_content(img, 50, 10));
    // Center should be empty (no fill)
    ASSERT(!has_content(img, 50, 50));
}

TEST(relative_path_commands) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <path d="m10,10 l80,0 l0,80 l-80,0 z" fill="white"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    ASSERT(has_content(img, 50, 50));
    ASSERT(!has_content(img, 0, 0));
}

TEST(line_element) {
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <line x1="10" y1="50" x2="90" y2="50" stroke="white" stroke-width="6"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    // Middle of the line
    ASSERT(has_content(img, 50, 50));
    // Far from the line
    ASSERT(!has_content(img, 50, 10));
}

TEST(nested_groups) {
    const char* svg = R"SVG(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <g transform="translate(50,50)">
                <g transform="scale(2)">
                    <rect x="-10" y="-10" width="20" height="20" fill="white"/>
                </g>
            </g>
        </svg>
    )SVG";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    // Should be centered at 50,50, scaled 2x -> 40x40 px
    ASSERT(has_content(img, 50, 50));
    ASSERT(!has_content(img, 5, 5));
}

TEST(fill_rule_evenodd) {
    // Concentric squares with evenodd - inner should be empty
    const char* svg = R"(
        <svg width="100" height="100" xmlns="http://www.w3.org/2000/svg">
            <path d="M10,10 L90,10 L90,90 L10,90 Z M30,30 L70,30 L70,70 L30,70 Z"
                  fill="white" fill-rule="evenodd"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img);
    ASSERT(ok);
    // Outer ring should be filled
    ASSERT(has_content(img, 15, 15));
    // Inner rect should be empty (evenodd)
    ASSERT(!has_content(img, 50, 50));
}

TEST(complex_icon_svg) {
    // A simple gear/settings icon approximation
    const char* svg = R"(
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none"
             stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <circle cx="12" cy="12" r="3"/>
            <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06
                     a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09
                     A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83
                     l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09
                     A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83
                     l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09
                     a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83
                     l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09
                     a1.65 1.65 0 0 0-1.51 1z"/>
        </svg>
    )";
    ugui::SvgImage img;
    bool ok = ugui::load_svg_memory(svg, std::strlen(svg), img, 64, 64);
    ASSERT(ok);
    ASSERT(img.width == 64);
    ASSERT(img.height == 64);
    // fill=none so center is empty, but stroked circle at r=3 (scaled to ~8px radius)
    // Check the stroke ring: pixel at ~8px from center (32,32)
    bool found = false;
    for (ugui::u32 x = 20; x < 44 && !found; ++x)
        for (ugui::u32 y = 20; y < 44 && !found; ++y)
            if (has_content(img, x, y))
                found = true;
    ASSERT(found);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("SVG test suite\n");
    std::printf("==============\n");
    // Tests auto-register and run via static initialization
    std::printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
