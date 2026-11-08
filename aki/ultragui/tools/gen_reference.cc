// Prints the values the aki tests compare against, computed by the C++
// library itself. Build and run inside the repo's dev shell:
//
//   clang++ -std=c++20 -O2 -I. aki/ultragui/tools/gen_reference.cc \
//     ugui/core/from_chars.cc ugui/style/style.cc ugui/style/transition.cc \
//     ugui/animation/animator.cc -o /tmp/gen_reference && /tmp/gen_reference
//
// The output is pasted into tests/*_test.aki as tables; re-run it whenever
// the C++ changes.
#include <ugui/animation/animator.h>
#include <ugui/core/algorithm.h>
#include <ugui/core/color.h>
#include <ugui/core/from_chars_compat.h>
#include <ugui/render/vertex.h>
#include <ugui/style/style.h>
#include <ugui/style/transition.h>

#include <stdio.h>
#include <string.h>

using namespace ugui;

static u32 Bits(f32 v) {
  u32 u;
  memcpy(&u, &v, 4);
  return u;
}

static void FromChars() {
  const char* inputs[] = {
      "0",           "1",          "-1",          "0.1",         "3.4028235e38",
      "3.4028236e38", "1e39",      "1e-46",       "1.4e-45",     "1.17549435e-38",
      "7.006492321624085e-46",    "0.30000001192092896",        "16777217",
      "1.00000005960464477539",   "1.000000059604644775391",    "-0",
      "inf",         "-Infinity",  "nan",         "nan(123)",    "1e",
      "1e+",         ".5",         "5.",          ".",           "-.",
      "+1",          " 1",         "0x10",        "123abc",      "2.5e-3px",
      "000000000000000000000000000000000000000000000000000000000000001.5",
      "1.2345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678e2",
  };
  printf("// from_chars_f32: input, ptr, ec, value bits (value starts 0x3f800000)\n");
  for (const char* in : inputs) {
    f32 v = 1.0f;
    auto r = from_chars(in, in + strlen(in), v);
    printf("FC \"%s\" %ld %d 0x%08x\n", in, (long)(r.ptr - in), (int)r.ec, Bits(v));
  }
  const char* ints[] = {"0", "ff", "FFFFFFFF", "100000000", "12345g", "", "zz"};
  for (const char* in : ints) {
    u32 v = 7;
    auto r = from_chars(in, in + strlen(in), v, 16);
    printf("FU \"%s\" %ld %d %u\n", in, (long)(r.ptr - in), (int)r.ec, v);
  }
}

struct Item {
  int key;
  int id;
};

static void Sort() {
  // A deterministic sequence with many ties: n = 100, keys 0..6.
  Vector<Item> v;
  u32 x = 12345;
  for (int i = 0; i < 100; ++i) {
    x = x * 1103515245u + 12345u;
    v.push_back({static_cast<int>((x >> 16) % 7), i});
  }
  IntroSort(v, [](const Item& a, const Item& b) { return a.key < b.key; });
  printf("SORT100");
  for (auto& it : v) printf(" %d", it.id);
  printf("\n");
  // Small (insertion sort only) and a sorted-descending big run (heap path is
  // reached only with adversarial input; this covers median-of-three).
  Vector<Item> s;
  for (int i = 0; i < 12; ++i) s.push_back({(i * 5) % 3, i});
  IntroSort(s, [](const Item& a, const Item& b) { return a.key < b.key; });
  printf("SORT12");
  for (auto& it : s) printf(" %d", it.id);
  printf("\n");
  Vector<Item> d;
  for (int i = 0; i < 300; ++i) d.push_back({(300 - i) / 4, i});
  IntroSort(d, [](const Item& a, const Item& b) { return a.key < b.key; });
  printf("SORT300");
  for (auto& it : d) printf(" %d", it.id);
  printf("\n");
}

static void Easing() {
  const f32 ts[] = {-0.5f, 0.0f, 0.1f, 0.25f, 0.5f, 0.7f, 0.9f, 1.0f, 1.5f};
  const EasingType types[] = {EasingType::kLinear, EasingType::kEaseIn,
                              EasingType::kEaseOut, EasingType::kEaseInOut,
                              EasingType::kCubicBezier, EasingType::kSpring};
  Transition tr;
  for (EasingType type : types) {
    tr.easing = type;
    printf("EASE %d", (int)type);
    for (f32 t : ts) printf(" 0x%08x", Bits(EvalEasing(tr, t)));
    printf("\n");
  }
  tr.easing = EasingType::kSpring;
  tr.spring_damping = 40.0f;  // critically damped / overdamped branch
  printf("SPRING40");
  for (f32 t : ts) printf(" 0x%08x", Bits(EvalEasing(tr, t)));
  printf("\n");
}

static void Lerps() {
  Style a;
  a.background = Color::FromHex(0x102030);
  a.opacity = 0.25f;
  a.corner_radius = 3.0f;
  a.width = Length::Px(100);
  a.gradient_stop_count = 1;
  a.gradient_stops[0] = {0.1f, Color(1, 0, 0, 1)};
  Style b;
  b.background = Color(0.9f, 0.8f, 0.7f, 0.6f);
  b.opacity = 1.0f;
  b.corner_radius = 11.0f;
  b.width = Length::Px(250);
  b.gradient_stop_count = 3;
  b.gradient_stops[0] = {0.0f, Color(0, 1, 0, 1)};
  b.gradient_stops[1] = {0.5f, Color(0, 0, 1, 1)};
  b.gradient_stops[2] = {1.0f, Color(1, 1, 1, 0.5f)};
  b.flex_direction = FlexDirection::kColumn;
  for (f32 t : {0.3f, 0.5f, 0.77f}) {
    Style r = Style::Lerp(a, b, t);
    printf("LERP 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x %u %d 0x%08x 0x%08x\n",
           Bits(r.background.r), Bits(r.background.g), Bits(r.background.a),
           Bits(r.opacity), Bits(r.corner_radius), Bits(r.width.value),
           Bits(r.gradient_stops[2].position), r.gradient_stop_count,
           (int)r.flex_direction, Bits(r.gradient_stops[1].color.b),
           Bits(r.gradient_stops[2].color.a));
  }
}

static void Animation() {
  Style a;
  a.opacity = 0.0f;
  a.font_size = 10.0f;
  Style b;
  b.opacity = 1.0f;
  b.font_size = 30.0f;
  Style c;
  c.opacity = 0.5f;
  c.font_size = 20.0f;
  KeyframeAnimation anim;
  anim.widget_id = 7;
  anim.keyframes.push_back({0.0f, a});
  anim.keyframes.push_back({0.6f, b});
  anim.keyframes.push_back({1.0f, c});
  anim.duration = 2.0f;
  anim.delay = 0.25f;
  anim.easing = EasingType::kEaseInOut;
  anim.repeat_count = 2;
  anim.alternate = true;
  for (f64 t : {0.0, 0.3, 0.9, 1.6, 2.2, 3.1, 4.0, 4.3}) {
    bool done = false;
    Style s = anim.Evaluate(t, done);
    printf("KEYF %d 0x%08x 0x%08x\n", done ? 1 : 0, Bits(s.opacity),
           Bits(s.font_size));
  }
  StyleTransition st{3, a, b, Transition{}, 1.0, true};
  st.transition.duration = 0.5f;
  st.transition.delay = 0.1f;
  st.transition.easing = EasingType::kCubicBezier;
  for (f64 t : {1.05, 1.2, 1.35, 1.59, 1.6, 2.0}) {
    bool done = false;
    Style s = st.Evaluate(t, done);
    printf("TRANS %d 0x%08x 0x%08x\n", done ? 1 : 0, Bits(s.opacity),
           Bits(s.font_size));
  }
}

static void Pack() {
  const f32 vals[][4] = {{0, 0, 0, 0},       {1, 1, 1, 1},
                         {0.5f, 0.25f, 0.75f, 0.1f}, {0.999f, 0.001f, 0.5019608f, 0.5f}};
  for (auto& c : vals)
    printf("PACK 0x%08x\n", Vertex2D::PackColor(c[0], c[1], c[2], c[3]));
  printf("RADII 0x%08x 0x%08x\n", Vertex2D::PackRadii(1.4f, 2.5f, 255.4f, 300.0f),
         Vertex2D::PackRadii(7.5f));
}

int main() {
  FromChars();
  Sort();
  Easing();
  Lerps();
  Animation();
  Pack();
  return 0;
}
