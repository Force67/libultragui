// Prints the values aki/ultragui/tests/anim_test.aki compares against, from
// the C++ library: JSON parse results, parsed .uganim documents, pixel hashes
// of rendered frames, and VectorAnimation playback. Build and run in the dev
// shell, from the repository root:
//
//   clang++ -std=c++20 -O2 -I. aki/ultragui/tools/gen_anim_reference.cc \
//     ugui/anim/*.cc ugui/svg/*.cc ugui/core/from_chars.cc ugui/style/transition.cc \
//     -o /tmp/gen_anim_reference && /tmp/gen_anim_reference . > aki/ultragui/tests/data/anim_reference.txt
//
// (the library's own aarch64 build contracts a*b+c into fused multiply-adds
// the way akiko does; the tables were recorded on aarch64 Linux).
#include <ugui/anim/anim_types.h>
#include <ugui/anim/json.h>
#include <ugui/anim/vector_animation.h>
#include <ugui/render/texture_backend.h>

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

using namespace ugui;

namespace ugui {
bool parse_anim_data(const char* json_data, usize length, AnimDocument& out);
void render_anim_frame(const AnimDocument& doc, f32 time, u8* pixels, u32 width, u32 height);
}  // namespace ugui

static unsigned Bits(f32 v) {
  unsigned u;
  memcpy(&u, &v, 4);
  return u;
}

static unsigned long long Bits64(f64 v) {
  unsigned long long u;
  memcpy(&u, &v, 8);
  return u;
}

static unsigned long long Fnv(const u8* p, size_t n) {
  unsigned long long h = 1469598103934665603ull;
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 1099511628211ull;
  }
  return h;
}

static std::string ReadFile(const std::string& path) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) {
    fprintf(stderr, "cannot open %s\n", path.c_str());
    exit(1);
  }
  std::string s;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
  fclose(f);
  return s;
}

// ---------------------------------------------------------------------------
// JSON: a canonical serialization of what ParseJson read.
// ---------------------------------------------------------------------------

static std::string Hex(const char* p, size_t n) {
  std::string out = "x";  // never empty
  char buf[4];
  for (size_t i = 0; i < n; ++i) {
    snprintf(buf, sizeof buf, "%02x", (unsigned char)p[i]);
    out += buf;
  }
  return out;
}

static void Ser(const JsonValue& v, std::string& out) {
  char buf[64];
  switch (v.type) {
    case JsonValue::kNull: out += "null"; break;
    case JsonValue::kBool: out += v.bool_val ? "true" : "false"; break;
    case JsonValue::kNumber:
      snprintf(buf, sizeof buf, "n0x%016llx", Bits64(v.number_val));
      out += buf;
      break;
    case JsonValue::kString: {
      out += '"';
      for (size_t i = 0; i < v.string_val.size(); ++i) {
        char c = v.string_val[i];
        if (c == '\\' || c == '"') out += '\\';
        if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else if (c == '\r') out += "\\r";
        else out += c;
      }
      out += '"';
      break;
    }
    case JsonValue::kArray:
      out += '[';
      for (size_t i = 0; i < v.array_val.size(); ++i) {
        if (i) out += ',';
        Ser(v.array_val[i], out);
      }
      out += ']';
      break;
    case JsonValue::kObject:
      out += '{';
      for (size_t i = 0; i < v.object_keys.size(); ++i) {
        if (i) out += ',';
        out += '"';
        out += v.object_keys[i].c_str();
        out += "\":";
        Ser(v.object_vals[i], out);
      }
      out += '}';
      break;
  }
}

static void JsonCases() {
  const char* inputs[] = {
      "null", "true", "false", "tru", "nul", "fals", "0", "-0", "1", "-1.5", "1e3", "1E-2", "2.5e+2",
      "0.1", "123456789012345678901234567890", "1e400", "-1e400", "1e-400", ".5", "-", "-.5", "0x10",
      "-inf", "-infinity", "-nan", "-nan(12)", "1abc", "12,34", "1.2.3", "--1",
      "\"\"", "\"abc\"", "\"a\\nb\\tc\\rd\"", "\"q\\\"q\\\\q\\/q\"", "\"\\u0041\"", "\"\\x\"", "\"é—ü\"",
      "\"open", "\"esc\\", "\"a\\\"", "[]", "[1,2,3]", "[ 1 , 2 ]", "[1,]", "[,1]", "[1 2]", "[1", "[",
      "{}", "{\"a\":1}", "{ \"a\" : 1 , \"b\" : [true,null] }", "{\"a\":1,}", "{\"a\"}", "{\"a\":}", "{a:1}",
      "{\"a\":1 \"b\":2}", "{\"a\":1", "{", "{\"dup\":1,\"dup\":2}", "{\"k\":{\"n\":{\"m\":[[],{}]}}}",
      "  \t\r\n 7", "", "   ", "x", "[1] trailing", "{\"a\":1} {\"b\":2}", "nulll", "truefalse", "[true,false,null]",
      "{\"layers\":[{\"shape\":\"rect\",}", "[\"a\",\"b\\n\"]",
  };
  int idx = 0;
  for (const char* in : inputs) {
    JsonValue v;
    bool ok = ParseJson(in, strlen(in), v);
    std::string s;
    Ser(v, s);
    printf("JSON %d %s %d %s\n", idx++, Hex(in, strlen(in)).c_str(), ok ? 1 : 0, s.c_str());
  }
}

// ---------------------------------------------------------------------------
// Documents and frames
// ---------------------------------------------------------------------------

static void LayerDump(const AnimLayer& l, int depth, const char* name) {
  printf("LAYER %s %d %d shape=%d xywh %08x %08x %08x %08x r %08x cxy %08x %08x rxy %08x %08x cr %08x rot %08x s %08x %08x op %08x sw %08x tracks=%zu children=%zu fill=%s stroke=%s mask=%s d=%zu\n",
         name, depth, 0, (int)l.shape_type, Bits(l.x), Bits(l.y), Bits(l.w), Bits(l.h), Bits(l.r), Bits(l.cx),
         Bits(l.cy), Bits(l.rx), Bits(l.ry), Bits(l.corner_radius), Bits(l.rotation), Bits(l.scale_x),
         Bits(l.scale_y), Bits(l.opacity), Bits(l.stroke_width), l.tracks.size(), l.children.size(),
         l.fill_str.c_str(), l.stroke_str.c_str(), l.mask_layer.c_str(), l.path_data.size());
  for (auto& t : l.tracks) {
    printf("TRACK %s %d prop=%d n=%zu:", name, depth, (int)t.property, t.keyframes.size());
    for (auto& k : t.keyframes)
      printf(" [%08x %08x %08x %08x %08x %08x e%d]", Bits(k.t), Bits(k.value), Bits(k.color.r), Bits(k.color.g),
             Bits(k.color.b), Bits(k.color.a), (int)k.easing);
    printf("\n");
  }
  for (auto& c : l.children) LayerDump(c, depth + 1, name);
}

static void Document(const std::string& name, const std::string& src, const std::string& text,
                     std::vector<AnimDocument>& keep) {
  AnimDocument doc;
  bool ok = parse_anim_data(text.data(), text.size(), doc);
  printf("ANIMDOC %s %s ok=%d version=%u w=%08x h=%08x dur=%08x loop=%d layers=%zu\n", name.c_str(), src.c_str(), ok ? 1 : 0,
         doc.version, Bits(doc.width), Bits(doc.height), Bits(doc.duration), doc.loop ? 1 : 0, doc.layers.size());
  if (!ok) return;
  for (auto& l : doc.layers) LayerDump(l, 0, name.c_str());
  const f32 fractions[] = {0.0f, 0.1f, 0.25f, 0.333f, 0.5f, 0.6180339f, 0.75f, 0.999f, 1.0f, 1.5f, -0.2f};
  const u32 sizes[][2] = {{64, 64}, {100, 80}, {37, 53}};
  for (auto& sz : sizes) {
    std::vector<u8> pixels(sz[0] * sz[1] * 4);
    for (f32 fr : fractions) {
      f32 t = fr * doc.duration;
      render_anim_frame(doc, t, pixels.data(), sz[0], sz[1]);
      printf("FRAME %s %u %u %08x 0x%016llx\n", name.c_str(), sz[0], sz[1], Bits(t), Fnv(pixels.data(), pixels.size()));
    }
  }
}

// ---------------------------------------------------------------------------
// VectorAnimation playback through a recording backend.
// ---------------------------------------------------------------------------

struct Recorder : TextureBackend {
  int creates = 0, updates = 0, destroys = 0;
  unsigned long long last_hash = 0;
  u32 w = 0, h = 0;
  TextureId next = 7;
  TextureId CreateTexture(u32 width, u32 height, RHIFormat format, const void* pixels, RHIFilter filter) override {
    ++creates;
    w = width;
    h = height;
    last_hash = Fnv(static_cast<const u8*>(pixels), (size_t)width * height * 4);
    printf("  create %u %u fmt=%d filter=%d 0x%016llx\n", width, height, (int)format, (int)filter, last_hash);
    return next++;
  }
  void UpdateTexture(TextureId id, const void* pixels) override {
    ++updates;
    last_hash = Fnv(static_cast<const u8*>(pixels), (size_t)w * h * 4);
  }
  void DestroyTexture(TextureId id) override { ++destroys; printf("  destroy %llu\n", (unsigned long long)id); }
};

static void State(const char* tag, int step, const VectorAnimation& a, const Recorder& r) {
  printf("PLAY %s %d loaded=%d playing=%d tex=%llu w=%u h=%u speed=%08x dur=%016llx prog=%08x creates=%d updates=%d destroys=%d hash=0x%016llx\n",
         tag, step, a.IsLoaded() ? 1 : 0, a.IsPlaying() ? 1 : 0, (unsigned long long)a.texture(), a.width(),
         a.height(), Bits(a.speed()), Bits64(a.duration()), Bits(a.progress()), r.creates, r.updates, r.destroys,
         r.last_hash);
}

static void Playback(const std::string& root) {
  std::string shapes = ReadFile(root + "/aki/ultragui/tests/data/anim/shapes.uganim");
  std::string edge = ReadFile(root + "/aki/ultragui/tests/data/anim/edge.uganim");
  {
    Recorder r;
    VectorAnimation a;
    State("unloaded", 0, a, r);
    a.Play();
    a.Seek(0.5f);
    State("unloaded", 1, a, r);
    bool ok = a.LoadData(&r, shapes.data(), shapes.size(), 96, 72);
    printf("PLAYLOAD shapes %d\n", ok ? 1 : 0);
    State("shapes", 0, a, r);
    a.Update(0.5);  // not playing
    State("shapes", 1, a, r);
    a.Play();
    a.set_speed(1.5f);
    for (int i = 0; i < 70; ++i) {
      a.Update(0.037);
      State("shapes", 2 + i, a, r);
    }
    a.Seek(0.25f);
    a.Update(0.001);
    State("shapes", 100, a, r);
    a.Pause();
    a.Update(1.0);
    State("shapes", 101, a, r);
    a.Stop();
    State("shapes", 102, a, r);
    a.Play();
    a.Update(0.2);
    State("shapes", 103, a, r);
    a.set_speed(-1.0f);
    a.Update(0.3);
    State("shapes", 104, a, r);
    a.Update(5.0);
    State("shapes", 105, a, r);
    a.Unload();
    State("shapes", 106, a, r);
  }
  {
    Recorder r;
    VectorAnimation a;
    a.LoadData(&r, edge.data(), edge.size(), 50, 40);
    a.set_loop(false);
    a.Play();
    for (int i = 0; i < 100; ++i) {
      a.Update(0.0125);
      if (i % 9 == 0 || i > 78) State("edge", i, a, r);
    }
    a.Seek(0.4f);
    State("edge", 200, a, r);
    a.Play();
    a.Update(0.0);
    State("edge", 201, a, r);
    // Reload over a loaded animation destroys the old texture.
    a.LoadData(&r, shapes.data(), shapes.size(), 20, 20);
    State("edge", 202, a, r);
    bool ok = a.LoadData(&r, "{ not json", 10, 10, 10);
    printf("PLAYLOAD bad %d\n", ok ? 1 : 0);
    State("edge", 203, a, r);
  }
  {
    // No backend: still renders and plays, with no texture.
    VectorAnimation a;
    Recorder r;
    a.LoadData(nullptr, shapes.data(), shapes.size(), 32, 32);
    a.Play();
    a.Update(0.5);
    State("nobackend", 0, a, r);
  }
}

int main(int argc, char** argv) {
  std::string root = argc > 1 ? argv[1] : ".";
  JsonCases();
  std::vector<AnimDocument> keep;
  const char* files[] = {"aki/ultragui/tests/data/anim/shapes.uganim", "aki/ultragui/tests/data/anim/edge.uganim",
                         "examples/anim_demo/anims/checkmark.uganim", "examples/anim_demo/anims/loading.uganim",
                         "examples/anim_demo/anims/pulse.uganim", "examples/anim_demo/anims/spinner.uganim"};
  for (const char* f : files) {
    std::string name = f;
    name = name.substr(name.rfind('/') + 1);
    Document(name, std::string("file:") + f, ReadFile(root + "/" + f), keep);
  }
  const char* bad[] = {"", "[]", "{}", "{\"layers\":[]}", "{ not json", "{\"layers\":[{}]}",
                       "{\"layers\":[{\"shape\":\"rect\"}], \"duration\": 0}",
                       "{\"layers\":[{\"shape\":\"rect\"}], \"duration\": -2, \"version\": -3, \"width\": \"x\"}",
                       "{\"layers\":[{\"shape\":\"rect\"}], \"version\": 1e20}"};
  int i = 0;
  for (const char* b : bad)
    Document(std::string("bad") + std::to_string(i++), "inline:" + Hex(b, strlen(b)), b, keep);
  Playback(root);
  return 0;
}
