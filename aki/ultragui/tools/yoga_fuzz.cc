// The test oracle for the aki port's flexbox engine (src/layout/flex_tree.aki,
// which replaced yoga there): generates random layout trees over the style
// space libultragui can write, runs them through real yoga (third_party/yoga),
// and prints a trace of every operation and every result.
// tests/yoga_fuzz_test.aki replays the trace on the engine and compares each
// result float bit for bit. It builds and runs this tool itself; by hand, in
// the dev shell from the repository root:
//
//   clang++ -std=c++20 -O2 -Ithird_party/yoga aki/ultragui/tools/yoga_fuzz.cc \
//     third_party/yoga/yoga/*.cpp third_party/yoga/yoga/*/*.cpp -o /tmp/yoga_fuzz
//   /tmp/yoga_fuzz SEED TREES > /tmp/yoga.trace
//   (cd aki/ultragui && akiko test tests/yoga_fuzz_test.aki -- /tmp/yoga.trace)
//
// It must be clang, as for the C++ library: clang contracts a*b+c into a
// fused multiply-add only inside one expression (-ffp-contract=on), which is
// the rule akiko follows; gcc's default contracts across statements too.
//
// What it generates is what ugui/layout/layout.cc can hand yoga: a style is
// drawn at libultragui's level (lengths in px, %, vw, vh, frac or auto; a ui
// scale; min/max that mean "none" at 0 and 1e6) and written through a copy of
// layout.cc's apply_style, so the yoga setters see the values, in the order,
// that libultragui produces. Trees are retained across passes as layout.cc
// retains them: a pass re-applies the styles that changed (all of them when
// the viewport or scale moved), sets each measured leaf's intrinsic size and
// marks it dirty when it changed, and calculates. Measured leaves report a
// fixed size whatever the constraints, as libultragui's measure function
// does. Collapsed nodes are display: none leaves, as layout_tree builds them,
// with a few display: none nodes that keep children for good measure.
//
// Trace format, one operation per line:
//   T <i>                 a new tree (the old one is dropped)
//   N <count>             add count nodes (0 is the root)
//   C <owner> <child> <i> YGNodeInsertChild
//   M <n>                 YGNodeSetMeasureFunc
//   I <n> <w> <h>         the size n's measure function reports from now on
//   D <n>                 YGNodeMarkDirty
//   S <n> <34 fields>     apply_style's setter calls, in its order:
//         flex-direction justify align-items align-self align-content wrap
//         overflow position-type display  (codes, see kFd... below)
//         grow shrink basis width height min-w min-h max-w max-h
//         margin T R B L, padding T R B L, gap all row column, aspect-ratio,
//         position T R B L
//       A length is `a` (auto), `<v>` (points) or `<v>%`; numbers are floats.
//       NaN ("nan") is the undefined yoga's setters turn it into.
//   L <w> <h>             YGNodeCalculateLayout(root, w, h, LTR)
//   R <n> <12 values>     n's results after the last L: left top width
//       height, margin L T R B, padding L T R B, each the shortest decimal
//       that reads back to the same float, or nan:<hex bits>
//   E                     end of the trace
#include <yoga/Yoga.h>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

// --- Deterministic randomness (splitmix64: the same on every platform) -------

struct Rng {
  uint64_t s;
  uint64_t Next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
  // [0, n)
  uint32_t Below(uint32_t n) { return static_cast<uint32_t>(Next() % n); }
  bool Chance(uint32_t percent) { return Below(100) < percent; }
  template <size_t N>
  float Pick(const float (&v)[N]) {
    return v[Below(N)];
  }
};

// --- libultragui's side: ugui::Length / ugui::Style, the parts layout reads ---

enum Unit { kPx, kPercent, kVw, kVh, kFrac, kAuto };

struct Len {
  Unit unit = kPx;
  float value = 0.0f;
  // Length::Resolve
  float Resolve(float parent, float vw, float vh, bool for_height) const {
    switch (unit) {
      case kPx:
        return value;
      case kPercent:
        return value * 0.01f * parent;
      case kVw:
        return value * 0.01f * vw;
      case kVh:
        return value * 0.01f * vh;
      case kFrac:
        return value * (for_height ? vh : vw);
      case kAuto:
        return 0.0f;
    }
    return 0.0f;
  }
  bool IsAuto() const { return unit == kAuto; }
};

Len Auto() { return Len{kAuto, 0.0f}; }
Len Px(float v) { return Len{kPx, v}; }

// The enums in ugui's order (FlexDirection, JustifyContent, ...).
enum FD { kRow, kColumn, kRowReverse, kColumnReverse };
enum JC { kJStart, kJEnd, kJCenter, kJBetween, kJAround, kJEvenly };
enum AI { kAStart, kAEnd, kACenter, kAStretch, kABaseline };
enum AS { kSAuto, kSStart, kSEnd, kSCenter, kSStretch };
enum AC { kCStart, kCEnd, kCCenter, kCStretch, kCBetween, kCAround, kCEvenly };
enum WR { kNoWrap, kWrap, kWrapReverse };
enum OV { kVisible, kHidden, kScroll };
enum PO { kRelative, kAbsolute, kSticky };

struct UStyle {
  int fd = kRow, jc = kJStart, ai = kAStretch, as = kSAuto, ac = kCStart;
  int wr = kNoWrap, ov = kVisible, pos = kRelative;
  int collapsed = 0;  // an int: UStyle has no padding, so memcmp compares it
  float grow = 0.0f, shrink = 1.0f;
  Len basis = Auto(), w = Auto(), h = Auto();
  Len minw = Px(0), minh = Px(0), maxw = Px(1e6f), maxh = Px(1e6f);
  float margin[4] = {};   // top right bottom left
  float padding[4] = {};  // top right bottom left
  float gap = 0.0f, row_gap = -1.0f, column_gap = -1.0f;
  float aspect_ratio = 0.0f;
  Len top = Auto(), right = Auto(), bottom = Auto(), left = Auto();
};

// --- Trace output ---------------------------------------------------------------

std::string g_out;

void Emit(const char* s) { g_out += s; }

void EmitFloat(float v) {
  if (std::isnan(v)) {
    g_out += "nan";
    return;
  }
  char buf[64];
  auto r = std::to_chars(buf, buf + sizeof buf, v);
  g_out.append(buf, r.ptr);
}

void EmitResult(float v) {
  g_out += ' ';
  if (std::isnan(v)) {
    uint32_t bits;
    memcpy(&bits, &v, 4);
    char buf[32];
    snprintf(buf, sizeof buf, "nan:%08x", bits);
    g_out += buf;
    return;
  }
  EmitFloat(v);
}

void EmitInt(long v) { g_out += std::to_string(v); }

void Field(float v) {
  g_out += ' ';
  EmitFloat(v);
}

void FieldInt(int v) {
  g_out += ' ';
  EmitInt(v);
}

// --- apply_style, as ugui/layout/layout.cc writes it, recording each call -------

// The trace's codes are the aki engine's enum order (layout::flex_style).
int CodeFd(int d) {
  switch (d) {
    case kRow:
      return 2;
    case kColumn:
      return 0;
    case kRowReverse:
      return 3;
    case kColumnReverse:
      return 1;
  }
  return 0;
}
YGFlexDirection YgFd(int d) {
  switch (d) {
    case kRow:
      return YGFlexDirectionRow;
    case kColumn:
      return YGFlexDirectionColumn;
    case kRowReverse:
      return YGFlexDirectionRowReverse;
    case kColumnReverse:
      return YGFlexDirectionColumnReverse;
  }
  return YGFlexDirectionRow;
}
// FlexJustify: FlexStart Center FlexEnd SpaceBetween SpaceAround SpaceEvenly
int CodeJc(int j) {
  static const int k[] = {0, 2, 1, 3, 4, 5};
  return k[j];
}
YGJustify YgJc(int j) {
  static const YGJustify k[] = {YGJustifyFlexStart,    YGJustifyFlexEnd,
                                YGJustifyCenter,       YGJustifySpaceBetween,
                                YGJustifySpaceAround,  YGJustifySpaceEvenly};
  return k[j];
}
// FlexAlign: Auto FlexStart Center FlexEnd Stretch Baseline SpaceBetween
// SpaceAround SpaceEvenly (YGAlign's order)
YGAlign YgAi(int a) {
  static const YGAlign k[] = {YGAlignFlexStart, YGAlignFlexEnd, YGAlignCenter,
                              YGAlignStretch, YGAlignBaseline};
  return k[a];
}
YGAlign YgAs(int a) {
  static const YGAlign k[] = {YGAlignAuto, YGAlignFlexStart, YGAlignFlexEnd,
                              YGAlignCenter, YGAlignStretch};
  return k[a];
}
YGAlign YgAc(int a) {
  static const YGAlign k[] = {YGAlignFlexStart,     YGAlignFlexEnd,
                              YGAlignCenter,        YGAlignStretch,
                              YGAlignSpaceBetween,  YGAlignSpaceAround,
                              YGAlignSpaceEvenly};
  return k[a];
}
int CodeAlign(YGAlign a) { return static_cast<int>(a); }

enum class Prop { kWidth, kHeight, kMinWidth, kMinHeight, kMaxWidth, kMaxHeight };

void SetPx(YGNodeRef yg, Prop p, float v) {
  switch (p) {
    case Prop::kWidth:
      YGNodeStyleSetWidth(yg, v);
      break;
    case Prop::kHeight:
      YGNodeStyleSetHeight(yg, v);
      break;
    case Prop::kMinWidth:
      YGNodeStyleSetMinWidth(yg, v);
      break;
    case Prop::kMinHeight:
      YGNodeStyleSetMinHeight(yg, v);
      break;
    case Prop::kMaxWidth:
      YGNodeStyleSetMaxWidth(yg, v);
      break;
    case Prop::kMaxHeight:
      YGNodeStyleSetMaxHeight(yg, v);
      break;
  }
  Field(v);
}

void SetPct(YGNodeRef yg, Prop p, float v) {
  switch (p) {
    case Prop::kWidth:
      YGNodeStyleSetWidthPercent(yg, v);
      break;
    case Prop::kHeight:
      YGNodeStyleSetHeightPercent(yg, v);
      break;
    case Prop::kMinWidth:
      YGNodeStyleSetMinWidthPercent(yg, v);
      break;
    case Prop::kMinHeight:
      YGNodeStyleSetMinHeightPercent(yg, v);
      break;
    case Prop::kMaxWidth:
      YGNodeStyleSetMaxWidthPercent(yg, v);
      break;
    case Prop::kMaxHeight:
      YGNodeStyleSetMaxHeightPercent(yg, v);
      break;
  }
  Field(v);
  g_out += '%';
}

void SetAuto(YGNodeRef yg, Prop p) {
  // Only width and height have one; the generator never makes an auto min or
  // max with a positive value, the one way layout.cc would skip the call.
  if (p == Prop::kWidth) YGNodeStyleSetWidthAuto(yg);
  if (p == Prop::kHeight) YGNodeStyleSetHeightAuto(yg);
  g_out += " a";
}

void SetLength(YGNodeRef yg, const Len& len, float vw, float vh, float scale,
               Prop p, bool height_axis) {
  switch (len.unit) {
    case kPx:
      SetPx(yg, p, len.value * scale);
      break;
    case kPercent:
      SetPct(yg, p, len.value);
      break;
    case kVw:
      SetPx(yg, p, len.value * 0.01f * vw);
      break;
    case kVh:
      SetPx(yg, p, len.value * 0.01f * vh);
      break;
    case kFrac:
      SetPx(yg, p, len.value * (height_axis ? vh : vw));
      break;
    case kAuto:
      SetAuto(yg, p);
      break;
  }
}

void ApplyStyle(YGNodeRef yg, int n, const UStyle& s, float vw, float vh,
                float sc) {
  Emit("S ");
  EmitInt(n);
  YGNodeStyleSetFlexDirection(yg, YgFd(s.fd));
  FieldInt(CodeFd(s.fd));
  YGNodeStyleSetJustifyContent(yg, YgJc(s.jc));
  FieldInt(CodeJc(s.jc));
  YGNodeStyleSetAlignItems(yg, YgAi(s.ai));
  FieldInt(CodeAlign(YgAi(s.ai)));
  YGNodeStyleSetAlignSelf(yg, YgAs(s.as));
  FieldInt(CodeAlign(YgAs(s.as)));
  YGNodeStyleSetAlignContent(yg, YgAc(s.ac));
  FieldInt(CodeAlign(YgAc(s.ac)));
  static const YGWrap kWraps[] = {YGWrapNoWrap, YGWrapWrap, YGWrapWrapReverse};
  YGNodeStyleSetFlexWrap(yg, kWraps[s.wr]);
  FieldInt(s.wr);
  static const YGOverflow kOverflows[] = {YGOverflowVisible, YGOverflowHidden,
                                          YGOverflowScroll};
  YGNodeStyleSetOverflow(yg, kOverflows[s.ov]);
  FieldInt(s.ov);
  const bool absolute = s.pos == kAbsolute;  // sticky is relative to yoga
  YGNodeStyleSetPositionType(
      yg, absolute ? YGPositionTypeAbsolute : YGPositionTypeRelative);
  FieldInt(absolute ? 1 : 0);
  YGNodeStyleSetDisplay(yg, s.collapsed ? YGDisplayNone : YGDisplayFlex);
  FieldInt(s.collapsed ? 1 : 0);

  YGNodeStyleSetFlexGrow(yg, s.grow);
  Field(s.grow);
  YGNodeStyleSetFlexShrink(yg, s.shrink);
  Field(s.shrink);
  switch (s.basis.unit) {
    case kAuto:
      YGNodeStyleSetFlexBasisAuto(yg);
      g_out += " a";
      break;
    case kPercent:
      YGNodeStyleSetFlexBasisPercent(yg, s.basis.value);
      Field(s.basis.value);
      g_out += '%';
      break;
    default: {
      const float v =
          s.basis.Resolve(0, vw, vh,
                          s.fd == kColumn || s.fd == kColumnReverse) *
          sc;
      YGNodeStyleSetFlexBasis(yg, v);
      Field(v);
      break;
    }
  }

  SetLength(yg, s.w, vw, vh, sc, Prop::kWidth, false);
  SetLength(yg, s.h, vw, vh, sc, Prop::kHeight, true);
  if (s.minw.value > 0.0f)
    SetLength(yg, s.minw, vw, vh, sc, Prop::kMinWidth, false);
  else
    SetPx(yg, Prop::kMinWidth, YGUndefined);
  if (s.minh.value > 0.0f)
    SetLength(yg, s.minh, vw, vh, sc, Prop::kMinHeight, true);
  else
    SetPx(yg, Prop::kMinHeight, YGUndefined);
  if (s.maxw.value < 1e5f)
    SetLength(yg, s.maxw, vw, vh, sc, Prop::kMaxWidth, false);
  else
    SetPx(yg, Prop::kMaxWidth, YGUndefined);
  if (s.maxh.value < 1e5f)
    SetLength(yg, s.maxh, vw, vh, sc, Prop::kMaxHeight, true);
  else
    SetPx(yg, Prop::kMaxHeight, YGUndefined);

  static const YGEdge kEdges[] = {YGEdgeTop, YGEdgeRight, YGEdgeBottom,
                                  YGEdgeLeft};
  for (int e = 0; e < 4; ++e) {
    const float v = s.margin[e] * sc;
    YGNodeStyleSetMargin(yg, kEdges[e], v);
    Field(v);
  }
  for (int e = 0; e < 4; ++e) {
    const float v = s.padding[e] * sc;
    YGNodeStyleSetPadding(yg, kEdges[e], v);
    Field(v);
  }

  const float gap_all = s.gap > 0.0f ? s.gap * sc : YGUndefined;
  const float gap_row = s.row_gap >= 0.0f ? s.row_gap * sc : YGUndefined;
  const float gap_col =
      s.column_gap >= 0.0f ? s.column_gap * sc : YGUndefined;
  YGNodeStyleSetGap(yg, YGGutterAll, gap_all);
  Field(gap_all);
  YGNodeStyleSetGap(yg, YGGutterRow, gap_row);
  Field(gap_row);
  YGNodeStyleSetGap(yg, YGGutterColumn, gap_col);
  Field(gap_col);

  const float ar = s.aspect_ratio > 0.0f ? s.aspect_ratio : YGUndefined;
  YGNodeStyleSetAspectRatio(yg, ar);
  Field(ar);

  const float top =
      !s.top.IsAuto() ? s.top.Resolve(0, vw, vh, true) * sc : YGUndefined;
  const float right = !s.right.IsAuto()
                          ? s.right.Resolve(0, vw, vh, false) * sc
                          : YGUndefined;
  const float bottom = !s.bottom.IsAuto()
                           ? s.bottom.Resolve(0, vw, vh, true) * sc
                           : YGUndefined;
  const float left =
      !s.left.IsAuto() ? s.left.Resolve(0, vw, vh, false) * sc : YGUndefined;
  YGNodeStyleSetPosition(yg, YGEdgeTop, top);
  Field(top);
  YGNodeStyleSetPosition(yg, YGEdgeRight, right);
  Field(right);
  YGNodeStyleSetPosition(yg, YGEdgeBottom, bottom);
  Field(bottom);
  YGNodeStyleSetPosition(yg, YGEdgeLeft, left);
  Field(left);
  Emit("\n");
}

// --- Random styles ---------------------------------------------------------------

// A pixel amount the way styles carry them: whole and half pixels, two
// decimals, products of a scale, and the odd large or negative one.
float RandPx(Rng& r, float hi) {
  switch (r.Below(8)) {
    case 0:
    case 1:
      return static_cast<float>(r.Below(static_cast<uint32_t>(hi) + 1));
    case 2:
      return static_cast<float>(r.Below(static_cast<uint32_t>(hi) + 1)) + 0.5f;
    case 3:
      return static_cast<float>(r.Below(static_cast<uint32_t>(hi * 100) + 1)) /
             100.0f;
    case 4: {
      static const float kScales[] = {1.25f, 1.5f, 0.75f, 1.3333f, 0.6667f};
      return static_cast<float>(r.Below(static_cast<uint32_t>(hi) + 1)) *
             r.Pick(kScales);
    }
    case 5:
      return static_cast<float>(r.Below(40));
    case 6: {
      static const float kOdd[] = {0.0f, 1.0f, 2.0f, 0.1f, 0.333333f, 1e-4f,
                                   2047.0f, 2047.5f, 4000.0f, 12345.678f};
      return r.Pick(kOdd);
    }
    default:
      return static_cast<float>(r.Below(1000)) / 7.0f;
  }
}

float RandPercent(Rng& r) {
  static const float kCommon[] = {100.0f, 50.0f, 25.0f, 33.333f, 66.667f,
                                  10.0f,  75.0f, 0.0f,  120.0f,  12.5f};
  if (r.Chance(60)) return r.Pick(kCommon);
  return static_cast<float>(r.Below(10001)) / 100.0f;
}

// A width or height (or basis, or inset) length.
Len RandSize(Rng& r, float px_hi, bool allow_auto) {
  const uint32_t k = r.Below(100);
  if (allow_auto && k < 45) return Auto();
  if (k < 75) return Px(RandPx(r, px_hi));
  if (k < 87) return Len{kPercent, RandPercent(r)};
  if (k < 93) return Len{kVw, static_cast<float>(r.Below(101))};
  if (k < 97) return Len{kVh, static_cast<float>(r.Below(101))};
  static const float kFracs[] = {0.5f, 0.25f, 0.1f, 1.0f, 0.333f};
  return Len{kFrac, r.Pick(kFracs)};
}

float RandSigned(Rng& r, float hi, uint32_t negative_percent) {
  const float v = RandPx(r, hi);
  return r.Chance(negative_percent) ? -v : v;
}

void RandMinMax(Rng& r, UStyle& s) {
  // Mostly unconstrained (min 0, max 1e6); otherwise a point or percent
  // value, sometimes equal (yoga then treats the size as fixed), sometimes
  // conflicting.
  auto bound = [&](Len& min, Len& max) {
    min = Px(0);
    max = Px(1e6f);
    const uint32_t k = r.Below(100);
    if (k < 55) return;
    if (k < 70) {
      min = RandSize(r, 300, false);
    } else if (k < 85) {
      max = RandSize(r, 400, false);
    } else if (k < 92) {
      min = RandSize(r, 300, false);
      max = RandSize(r, 300, false);
    } else {
      min = RandSize(r, 300, false);
      max = min;
      if (r.Chance(30) && max.unit == kPx) max.value += 0.00005f;
    }
  };
  bound(s.minw, s.maxw);
  bound(s.minh, s.maxh);
}

void RandStyle(Rng& r, UStyle& s, bool leaf, bool root) {
  static const int kFds[] = {kRow, kColumn, kColumn, kRow, kRowReverse,
                             kColumnReverse};
  s.fd = kFds[r.Below(6)];
  s.jc = r.Chance(55) ? kJStart : static_cast<int>(r.Below(6));
  s.ai = r.Chance(45) ? kAStretch : static_cast<int>(r.Below(5));
  s.as = r.Chance(75) ? kSAuto : static_cast<int>(r.Below(5));
  s.ac = r.Chance(55) ? kCStart : static_cast<int>(r.Below(7));
  s.wr = r.Chance(65) ? kNoWrap : static_cast<int>(r.Below(3));
  s.ov = r.Chance(70) ? kVisible : static_cast<int>(r.Below(3));
  s.pos = root ? kRelative : (r.Chance(82) ? kRelative : (r.Chance(70) ? kAbsolute : kSticky));
  s.collapsed = !root && leaf && r.Chance(7) ? 1 : 0;
  static const float kGrow[] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 2.0f, 0.5f, 3.0f, 0.25f};
  static const float kShrink[] = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.5f};
  s.grow = r.Pick(kGrow);
  s.shrink = r.Pick(kShrink);
  s.basis = r.Chance(70) ? Auto() : RandSize(r, 300, false);
  s.w = RandSize(r, leaf ? 300 : 900, true);
  s.h = RandSize(r, leaf ? 200 : 700, true);
  RandMinMax(r, s);
  for (int e = 0; e < 4; ++e) {
    s.margin[e] = r.Chance(55) ? 0.0f : RandSigned(r, 30, 15);
    s.padding[e] = r.Chance(55) ? 0.0f : RandSigned(r, 30, 5);
  }
  s.gap = r.Chance(60) ? 0.0f : RandSigned(r, 24, 5);
  s.row_gap = r.Chance(75) ? -1.0f : RandPx(r, 24);
  s.column_gap = r.Chance(75) ? -1.0f : RandPx(r, 24);
  static const float kRatios[] = {1.0f, 0.5f, 2.0f, 16.0f / 9.0f, 1.3333334f, 0.75f, 3.0f};
  s.aspect_ratio = r.Chance(85) ? 0.0f : r.Pick(kRatios);
  const uint32_t inset = s.pos == kAbsolute ? 45 : 12;
  auto rand_inset = [&]() {
    if (!r.Chance(inset)) return Auto();
    Len l = RandSize(r, 200, false);
    if (r.Chance(20)) l.value = -l.value;
    return l;
  };
  s.top = rand_inset();
  s.right = rand_inset();
  s.bottom = rand_inset();
  s.left = rand_inset();
}

// One property of a style, changed the way an interaction changes it.
void Mutate(Rng& r, UStyle& s, bool leaf, bool root) {
  switch (r.Below(16)) {
    case 0: {
      UStyle fresh;
      RandStyle(r, fresh, leaf, root);
      s = fresh;
      break;
    }
    case 1:
      s.w = RandSize(r, leaf ? 300 : 900, true);
      break;
    case 2:
      s.h = RandSize(r, leaf ? 200 : 700, true);
      break;
    case 3:
      s.grow = s.grow == 0.0f ? 1.0f : 0.0f;
      break;
    case 4:
      s.shrink = s.shrink == 0.0f ? 1.0f : 0.0f;
      break;
    case 5:
      s.padding[r.Below(4)] = RandPx(r, 20);
      break;
    case 6:
      s.margin[r.Below(4)] = RandSigned(r, 20, 15);
      break;
    case 7:
      s.gap = r.Chance(50) ? 0.0f : RandPx(r, 20);
      break;
    case 8:
      if (!root && leaf) s.collapsed = !s.collapsed;
      break;
    case 9:
      s.fd = static_cast<int>(r.Below(4));
      break;
    case 10:
      s.wr = static_cast<int>(r.Below(3));
      break;
    case 11:
      RandMinMax(r, s);
      break;
    case 12:
      s.basis = r.Chance(50) ? Auto() : RandSize(r, 300, false);
      break;
    case 13:
      s.ai = static_cast<int>(r.Below(5));
      s.jc = static_cast<int>(r.Below(6));
      break;
    case 14:
      if (!root) s.pos = s.pos == kAbsolute ? kRelative : kAbsolute;
      break;
    default:
      s.top = r.Chance(50) ? Auto() : Px(RandSigned(r, 50, 30));
      s.left = r.Chance(50) ? Auto() : Px(RandSigned(r, 50, 30));
      break;
  }
}

// A measured leaf's size: a shaped run of text, mostly.
void RandIntrinsic(Rng& r, float& w, float& h) {
  static const float kLine[] = {14.0f, 16.5f, 18.0f, 21.25f, 12.0f, 33.0f};
  w = r.Chance(10) ? 0.0f : RandPx(r, 260);
  h = r.Pick(kLine) * static_cast<float>(1 + r.Below(3));
  if (r.Chance(5)) h = 0.0f;
  if (w <= 0.0f && h <= 0.0f) h = 12.0f;  // layout.cc measures a leaf only
                                          // with some content
}

struct Viewport {
  float w, h, scale;
};

Viewport RandViewport(Rng& r) {
  static const float kSizes[][2] = {{1280, 720}, {1920, 1080}, {800, 600},
                                    {640, 480},  {400, 400},   {1024, 768},
                                    {360, 640},  {120, 80}};
  static const float kScales[] = {1.0f, 1.0f, 1.0f, 1.25f, 1.5f, 2.0f, 0.75f};
  Viewport v;
  const uint32_t k = r.Below(100);
  if (k < 70) {
    const auto& s = kSizes[r.Below(8)];
    v.w = s[0];
    v.h = s[1];
  } else if (k < 95) {
    v.w = static_cast<float>(r.Below(200000)) / 100.0f;
    v.h = static_cast<float>(r.Below(120000)) / 100.0f;
  } else {
    v.w = static_cast<float>(r.Below(3));
    v.h = static_cast<float>(r.Below(3));
  }
  v.scale = r.Pick(kScales);
  return v;
}

// --- Trees --------------------------------------------------------------------------

struct Node {
  int parent = -1;
  std::vector<int> children;
  UStyle style;
  bool measured = false;
  float iw = 0.0f, ih = 0.0f;
  YGNodeRef yg = nullptr;
};

// Nodes in depth-first preorder, as layout_tree numbers them.
void Grow(Rng& r, std::vector<Node>& nodes, int parent, int depth,
          int max_nodes) {
  const int me = static_cast<int>(nodes.size());
  nodes.push_back({});
  nodes[me].parent = parent;
  if (parent >= 0) nodes[parent].children.push_back(me);
  const uint32_t max_kids = depth == 0 ? 6 : depth < 3 ? 5 : depth < 5 ? 3 : 0;
  uint32_t kids = max_kids == 0 ? 0 : r.Below(max_kids + 1);
  if (depth == 0 && kids == 0 && r.Chance(80)) kids = 1 + r.Below(4);
  for (uint32_t i = 0; i < kids; ++i) {
    if (static_cast<int>(nodes.size()) >= max_nodes) break;
    Grow(r, nodes, me, depth + 1, max_nodes);
  }
}

void EmitResults(const std::vector<Node>& nodes) {
  for (size_t i = 0; i < nodes.size(); ++i) {
    YGNodeConstRef yg = nodes[i].yg;
    Emit("R ");
    EmitInt(static_cast<long>(i));
    EmitResult(YGNodeLayoutGetLeft(yg));
    EmitResult(YGNodeLayoutGetTop(yg));
    EmitResult(YGNodeLayoutGetWidth(yg));
    EmitResult(YGNodeLayoutGetHeight(yg));
    for (YGEdge e : {YGEdgeLeft, YGEdgeTop, YGEdgeRight, YGEdgeBottom})
      EmitResult(YGNodeLayoutGetMargin(yg, e));
    for (YGEdge e : {YGEdgeLeft, YGEdgeTop, YGEdgeRight, YGEdgeBottom})
      EmitResult(YGNodeLayoutGetPadding(yg, e));
    Emit("\n");
  }
}

YGSize MeasureIntrinsic(YGNodeConstRef node, float, YGMeasureMode, float,
                        YGMeasureMode) {
  const auto* n = static_cast<const Node*>(YGNodeGetContext(node));
  return {n->iw, n->ih};
}

bool SameStyle(const UStyle& a, const UStyle& b) {
  return memcmp(&a, &b, sizeof a) == 0;
}

void RunTree(Rng& r, YGConfigRef config, long index) {
  std::vector<Node> nodes;
  const int max_nodes = r.Chance(20) ? 4 : r.Chance(60) ? 16 : 40;
  Grow(r, nodes, -1, 0, max_nodes);
  // A few collapsed nodes keep their children (not something layout_tree
  // builds, but cheap to cover).
  for (size_t i = 0; i < nodes.size(); ++i) {
    const bool leaf = nodes[i].children.empty();
    RandStyle(r, nodes[i].style, leaf, i == 0);
    if (!leaf && i != 0 && r.Chance(2)) nodes[i].style.collapsed = 1;
    if (leaf && i != 0 && r.Chance(60)) {
      nodes[i].measured = true;
      RandIntrinsic(r, nodes[i].iw, nodes[i].ih);
    }
  }

  Emit("T ");
  EmitInt(index);
  Emit("\nN ");
  EmitInt(static_cast<long>(nodes.size()));
  Emit("\n");
  for (auto& n : nodes) {
    n.yg = YGNodeNewWithConfig(config);
    YGNodeSetContext(n.yg, &n);
  }
  for (size_t i = 0; i < nodes.size(); ++i) {
    for (size_t k = 0; k < nodes[i].children.size(); ++k) {
      YGNodeInsertChild(nodes[i].yg, nodes[nodes[i].children[k]].yg, k);
      Emit("C ");
      EmitInt(static_cast<long>(i));
      Emit(" ");
      EmitInt(nodes[i].children[k]);
      Emit(" ");
      EmitInt(static_cast<long>(k));
      Emit("\n");
    }
  }
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].measured) {
      YGNodeSetMeasureFunc(nodes[i].yg, MeasureIntrinsic);
      Emit("M ");
      EmitInt(static_cast<long>(i));
      Emit("\n");
    }
  }

  std::vector<UStyle> applied(nodes.size());
  std::vector<float> last_w(nodes.size(), 0.0f), last_h(nodes.size(), 0.0f);
  Viewport vp = RandViewport(r);
  Viewport applied_vp{0, 0, 0};
  const uint32_t passes = 1 + r.Below(7);
  for (uint32_t pass = 0; pass < passes; ++pass) {
    if (pass > 0) {
      if (r.Chance(30)) vp = RandViewport(r);
      // Interactions: restyle a few nodes, re-shape a few texts.
      const uint32_t edits = r.Below(4);
      for (uint32_t e = 0; e < edits; ++e) {
        const size_t i = r.Below(static_cast<uint32_t>(nodes.size()));
        Mutate(r, nodes[i].style, nodes[i].children.empty(), i == 0);
      }
      for (auto& n : nodes)
        if (n.measured && r.Chance(15)) RandIntrinsic(r, n.iw, n.ih);
    }
    const bool viewport_moved = !(applied_vp.w == vp.w && applied_vp.h == vp.h &&
                                  applied_vp.scale == vp.scale);
    applied_vp = vp;
    for (size_t i = 0; i < nodes.size(); ++i) {
      Node& n = nodes[i];
      if (n.measured) {
        Emit("I ");
        EmitInt(static_cast<long>(i));
        Field(n.iw);
        Field(n.ih);
        Emit("\n");
      }
      // A refreshed node whose style did not move is skipped, as in
      // layout.cc; now and then one is written again anyway (a no-op).
      if (pass == 0 || viewport_moved || !SameStyle(applied[i], n.style) ||
          r.Chance(3)) {
        ApplyStyle(n.yg, static_cast<int>(i), n.style, vp.w, vp.h, vp.scale);
        applied[i] = n.style;
      }
      if (n.measured && (n.iw != last_w[i] || n.ih != last_h[i])) {
        last_w[i] = n.iw;
        last_h[i] = n.ih;
        YGNodeMarkDirty(n.yg);
        Emit("D ");
        EmitInt(static_cast<long>(i));
        Emit("\n");
      }
    }
    YGNodeCalculateLayout(nodes[0].yg, vp.w, vp.h, YGDirectionLTR);
    Emit("L");
    Field(vp.w);
    Field(vp.h);
    Emit("\n");
    EmitResults(nodes);
  }
  YGNodeFreeRecursive(nodes[0].yg);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s SEED TREES > trace\n", argv[0]);
    return 2;
  }
  const uint64_t seed = strtoull(argv[1], nullptr, 10);
  const long trees = strtol(argv[2], nullptr, 10);
  Rng r{seed};
  YGConfigRef config = YGConfigNew();
  g_out.reserve(1 << 24);
  for (long t = 0; t < trees; ++t) {
    RunTree(r, config, t);
    if (g_out.size() > (1 << 23)) {
      fwrite(g_out.data(), 1, g_out.size(), stdout);
      g_out.clear();
    }
  }
  Emit("E\n");
  fwrite(g_out.data(), 1, g_out.size(), stdout);
  YGConfigFree(config);
  return 0;
}
