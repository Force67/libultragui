// Prints a canonical text dump of the .ugui AST (and parse errors) produced
// by the C++ parser, for the aki parser's tests to compare with. Build and run
// inside the repo's dev shell:
//
//   clang++ -std=c++20 -O1 -I. aki/ultragui/tools/dump_ugui.cc \
//     ugui/idl/parser.cc ugui/core/from_chars.cc -o /tmp/dump_ugui
//   /tmp/dump_ugui --root <repo> file.ugui...     # parse files, one dump each
//   /tmp/dump_ugui --text 'source text'           # parse text as "<text>"
//
// The format is the one Dump in aki/ultragui/tests/idl_parser_test.aki
// writes: property maps in ascending key order, strings with backslash,
// double quote, newline and tab backslash-escaped, floats as 0x%08x bit
// patterns, every path with the --root prefix replaced by $ROOT.
#include <ugui/idl/parser.h>

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace ugui;

static std::string g_root;

static std::string Esc(const char* s, size_t n) {
  std::string out = "\"";
  for (size_t i = 0; i < n; ++i) {
    char c = s[i];
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out + "\"";
}

static std::string Q(const String& s) { return Esc(s.data(), s.size()); }

// Paths: the root prefix becomes $ROOT.
static std::string P(const String& s) {
  std::string v(s.data(), s.size());
  if (!g_root.empty() && v.compare(0, g_root.size(), g_root) == 0)
    v = "$ROOT" + v.substr(g_root.size());
  return Esc(v.data(), v.size());
}

static std::string Bits(f32 v) {
  unsigned u;
  memcpy(&u, &v, 4);
  char buf[16];
  snprintf(buf, sizeof buf, "0x%08x", u);
  return buf;
}

static void Pad(int depth) {
  for (int i = 0; i < depth; ++i) fputs("  ", stdout);
}

static void Props(const HashMap<String, String>& m, int depth) {
  std::vector<std::pair<std::string, std::string>> v;
  for (const auto& [k, val] : m) v.push_back({std::string(k.data(), k.size()), std::string(val.data(), val.size())});
  std::sort(v.begin(), v.end());
  for (auto& [k, val] : v) {
    Pad(depth);
    printf("p %s = %s\n", Esc(k.data(), k.size()).c_str(), Esc(val.data(), val.size()).c_str());
  }
}

static void State(const UguiNode::StateBlock& sb, int depth) {
  Pad(depth);
  printf("state %s\n", Q(sb.state).c_str());
  Props(sb.properties, depth + 1);
}

static void Node(const UguiNode& n, int depth) {
  Pad(depth);
  printf("node type=%s name=%s line=%u\n", Q(n.type).c_str(), Q(n.name).c_str(), n.source_line);
  Props(n.properties, depth + 1);
  for (auto& sb : n.state_blocks) State(sb, depth + 1);
  for (auto& kb : n.keyframe_blocks) {
    Pad(depth + 1);
    printf("keyframes %s\n", Q(kb.name).c_str());
    Props(kb.properties, depth + 2);
    for (auto& stop : kb.stops) {
      Pad(depth + 2);
      printf("stop %s\n", Bits(stop.percent).c_str());
      Props(stop.properties, depth + 3);
    }
  }
  for (auto& mq : n.media_queries) {
    Pad(depth + 1);
    printf("media %s %s\n", Q(mq.condition).c_str(), Bits(mq.value).c_str());
    Props(mq.properties, depth + 2);
  }
  for (auto& c : n.children) Node(c, depth + 1);
}

static void Dump(const char* label, bool ok, const UguiDocument& doc, const Vector<ParseError>& errors) {
  printf("== %s ok=%d\n", label, ok ? 1 : 0);
  printf("source %s\n", P(doc.source_path).c_str());
  for (auto& i : doc.imports) printf("import %s\n", Q(i).c_str());
  for (auto& sc : doc.style_classes) {
    printf("class %s\n", Q(sc.name).c_str());
    Props(sc.properties, 1);
    for (auto& sb : sc.state_blocks) State(sb, 1);
  }
  for (auto& c : doc.components) {
    printf("component %s line=%u\n", Q(c.name).c_str(), c.source_line);
    Props(c.props, 1);
    Node(c.root, 1);
  }
  for (auto& r : doc.roots) Node(r, 0);
  for (auto& e : errors)
    printf("error file=%s line=%u col=%u msg=%s\n", P(e.file).c_str(), e.line, e.column, Q(e.message).c_str());
}

int main(int argc, char** argv) {
  bool text = false;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--root") && i + 1 < argc) {
      g_root = argv[++i];
    } else if (!strcmp(argv[i], "--text")) {
      text = true;
    } else if (text) {
      UguiDocument doc;
      Vector<ParseError> errors;
      bool ok = ParseUgui(argv[i], strlen(argv[i]), "<text>", doc, errors);
      Dump("<text>", ok, doc, errors);
    } else {
      UguiDocument doc;
      Vector<ParseError> errors;
      bool ok = ParseUguiFile(argv[i], doc, errors);
      std::string label = argv[i];
      if (!g_root.empty() && label.compare(0, g_root.size(), g_root) == 0) label = "$ROOT" + label.substr(g_root.size());
      Dump(label.c_str(), ok, doc, errors);
    }
  }
  return 0;
}
