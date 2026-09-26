#include <ugui/idl/parser.h>
#include <ugui/core/from_chars_compat.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace ugui {

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------

enum class TokenType : u8 {
  kIdentifier,
  kString,  // "quoted string"
  kNumber,
  kHexColor,  // #RRGGBB or #RRGGBBAA
  kColon,
  kSemicolon,
  kLBrace,
  kRBrace,
  kAt,  // @
  kEof,
  kError,
};

struct Token {
  TokenType type;
  String value;
  u32 line;
  u32 col;
};

class Lexer {
 public:
  Lexer(const char* src, usize len, const char* file)
      : src_(src), len_(len), pos_(0), line_(1), col_(1), file_(file) {}

  Token next() {
    skip_whitespace_and_comments();

    if (pos_ >= len_) return {TokenType::kEof, "", line_, col_};

    char c = src_[pos_];
    u32 tok_line = line_, tok_col = col_;

    if (c == '{') {
      advance();
      return {TokenType::kLBrace, "{", tok_line, tok_col};
    }
    if (c == '}') {
      advance();
      return {TokenType::kRBrace, "}", tok_line, tok_col};
    }
    if (c == ':') {
      advance();
      return {TokenType::kColon, ":", tok_line, tok_col};
    }
    if (c == ';') {
      advance();
      return {TokenType::kSemicolon, ";", tok_line, tok_col};
    }
    if (c == '@') {
      advance();
      return {TokenType::kAt, "@", tok_line, tok_col};
    }

    if (c == '"') return lex_string(tok_line, tok_col);

    if (c == '#') return lex_hex_color(tok_line, tok_col);

    if (isdigit(c) ||
        (c == '-' && pos_ + 1 < len_ && isdigit(src_[pos_ + 1])) || c == '.')
      return lex_number(tok_line, tok_col);

    if (isalpha(c) || c == '_' || c == '-' || c == '$')
      return lex_identifier(tok_line, tok_col);

    advance();
    return {TokenType::kError, String(1, c), tok_line, tok_col};
  }

 private:
  void advance() {
    if (pos_ < len_) {
      if (src_[pos_] == '\n') {
        line_++;
        col_ = 1;
      } else {
        col_++;
      }
      pos_++;
    }
  }

  char peek() const { return pos_ < len_ ? src_[pos_] : 0; }

  void skip_whitespace_and_comments() {
    while (pos_ < len_) {
      if (isspace(src_[pos_])) {
        advance();
      } else if (pos_ + 1 < len_ && src_[pos_] == '/' &&
                 src_[pos_ + 1] == '/') {
        while (pos_ < len_ && src_[pos_] != '\n') advance();
      } else if (pos_ + 1 < len_ && src_[pos_] == '/' &&
                 src_[pos_ + 1] == '*') {
        advance();
        advance();
        while (pos_ + 1 < len_ && !(src_[pos_] == '*' && src_[pos_ + 1] == '/'))
          advance();
        advance();
        advance();
      } else {
        break;
      }
    }
  }

  Token lex_string(u32 line, u32 col) {
    advance();  // skip opening "
    String val;
    while (pos_ < len_ && src_[pos_] != '"') {
      if (src_[pos_] == '\\' && pos_ + 1 < len_) {
        advance();
        switch (src_[pos_]) {
          case 'n':
            val += '\n';
            break;
          case 't':
            val += '\t';
            break;
          case '"':
            val += '"';
            break;
          case '\\':
            val += '\\';
            break;
          default:
            val += src_[pos_];
            break;
        }
      } else {
        val += src_[pos_];
      }
      advance();
    }
    if (pos_ < len_) advance();  // skip closing "
    return {TokenType::kString, val, line, col};
  }

  Token lex_hex_color(u32 line, u32 col) {
    advance();  // skip #
    String val = "#";
    while (pos_ < len_ && isxdigit(src_[pos_])) {
      val += src_[pos_];
      advance();
    }
    return {TokenType::kHexColor, val, line, col};
  }

  Token lex_number(u32 line, u32 col) {
    String val;
    if (src_[pos_] == '-') {
      val += '-';
      advance();
    }
    while (pos_ < len_ && (isdigit(src_[pos_]) || src_[pos_] == '.')) {
      val += src_[pos_];
      advance();
    }
    // Unit suffix: px, %, vw, vh
    if (pos_ < len_) {
      if (src_[pos_] == '%') {
        val += '%';
        advance();
      } else if (pos_ + 1 < len_ && src_[pos_] == 'p' &&
                 src_[pos_ + 1] == 'x') {
        val += "px";
        advance();
        advance();
      } else if (pos_ + 1 < len_ && src_[pos_] == 'v' &&
                 src_[pos_ + 1] == 'w') {
        val += "vw";
        advance();
        advance();
      } else if (pos_ + 1 < len_ && src_[pos_] == 'v' &&
                 src_[pos_ + 1] == 'h') {
        val += "vh";
        advance();
        advance();
      } else if (pos_ + 1 < len_ && src_[pos_] == 'm' &&
                 src_[pos_ + 1] == 's') {
        val += "ms";
        advance();
        advance();
      } else if (src_[pos_] == 's' &&
                 (pos_ + 1 >= len_ || !isalpha(src_[pos_ + 1]))) {
        val += "s";
        advance();
      } else if (pos_ + 1 < len_ && src_[pos_] == 'f' &&
                 src_[pos_ + 1] == 'r') {
        val += "fr";
        advance();
        advance();
      }
    }
    return {TokenType::kNumber, val, line, col};
  }

  Token lex_identifier(u32 line, u32 col) {
    String val;
    // `$prop` references inside component bodies lex as one identifier
    if (pos_ < len_ && src_[pos_] == '$') {
      val += '$';
      advance();
    }
    while (pos_ < len_ &&
           (isalnum(src_[pos_]) || src_[pos_] == '_' || src_[pos_] == '-')) {
      val += src_[pos_];
      advance();
    }
    return {TokenType::kIdentifier, val, line, col};
  }

  const char* src_;
  usize len_;
  usize pos_;
  u32 line_;
  u32 col_;
  const char* file_;
};

// ---------------------------------------------------------------------------
// Recursive descent parser
// ---------------------------------------------------------------------------

class Parser {
 public:
  Parser(const char* src, usize len, const char* file)
      : lexer_(src, len, file), file_(file) {
    advance();
  }

  bool parse(UguiDocument& doc, Vector<ParseError>& errors) {
    errors_ = &errors;
    doc.source_path = file_;

    while (current_.type != TokenType::kEof) {
      // Top-level `class` blocks are style class declarations, not widgets.
      if (current_.type == TokenType::kIdentifier &&
          current_.value == "class") {
        auto sc = parse_style_class();
        if (!sc.name.empty()) doc.style_classes.push_back(ugui::move(sc));
        continue;
      }
      if (current_.type == TokenType::kIdentifier &&
          current_.value == "component") {
        auto comp = parse_component();
        if (!comp.name.empty()) doc.components.push_back(ugui::move(comp));
        continue;
      }
      if (current_.type == TokenType::kIdentifier &&
          current_.value == "import") {
        String path = parse_import();
        if (!path.empty()) doc.imports.push_back(ugui::move(path));
        continue;
      }
      auto node = parse_element();
      if (node.type.empty()) break;
      doc.roots.push_back(ugui::move(node));
    }

    return errors.empty();
  }

  UguiDocument::StyleClass parse_style_class() {
    UguiDocument::StyleClass sc;
    advance();  // skip 'class' identifier
    if (current_.type != TokenType::kIdentifier) {
      error("expected class name after 'class'");
      return sc;
    }
    sc.name = current_.value;
    advance();
    expect(TokenType::kLBrace);
    while (current_.type != TokenType::kRBrace &&
           current_.type != TokenType::kEof) {
      if (current_.type == TokenType::kColon) {
        sc.state_blocks.push_back(parse_state_block());
      } else if (current_.type == TokenType::kIdentifier) {
        Token id = current_;
        advance();
        if (current_.type == TokenType::kColon) {
          advance();  // skip ':'
          String value = parse_value();
          if (current_.type == TokenType::kSemicolon) advance();
          sc.properties[id.value] = value;
        } else {
          advance();
        }
      } else {
        advance();
      }
    }
    if (current_.type == TokenType::kRBrace) advance();
    return sc;
  }

  // component = 'component' name '{' ('prop' name [':' value] ';')* element '}'
  UguiDocument::Component parse_component() {
    UguiDocument::Component comp;
    comp.source_line = current_.line;
    advance();  // skip 'component'
    if (current_.type != TokenType::kIdentifier) {
      error("expected component name after 'component'");
      return comp;
    }
    comp.name = current_.value;
    advance();
    expect(TokenType::kLBrace);

    bool has_root = false;
    while (current_.type != TokenType::kRBrace &&
           current_.type != TokenType::kEof) {
      if (current_.type == TokenType::kIdentifier &&
          current_.value == "prop") {
        advance();  // skip 'prop'
        if (current_.type != TokenType::kIdentifier) {
          error("expected prop name after 'prop'");
          continue;
        }
        String prop_name = current_.value;
        advance();
        String default_value;
        if (current_.type == TokenType::kColon) {
          advance();
          default_value = parse_value();
        }
        if (current_.type == TokenType::kSemicolon) advance();
        comp.props[prop_name] = default_value;
      } else if (current_.type == TokenType::kIdentifier) {
        UguiNode el = parse_element();
        if (has_root) {
          errors_->push_back({"component '" + comp.name +
                                  "' must have exactly one root element",
                              file_, el.source_line, 0});
        } else {
          comp.root = ugui::move(el);
          has_root = true;
        }
      } else {
        advance();
      }
    }
    if (current_.type == TokenType::kRBrace) advance();
    if (!has_root)
      errors_->push_back({"component '" + comp.name + "' has no root element",
                          file_, comp.source_line, 0});
    return comp;
  }

  // import = 'import' string ';'
  String parse_import() {
    advance();  // skip 'import'
    String path;
    if (current_.type == TokenType::kString) {
      path = current_.value;
      advance();
    } else {
      error("expected \"path\" after 'import'");
    }
    if (current_.type == TokenType::kSemicolon) advance();
    return path;
  }

 private:
  void advance() { current_ = lexer_.next(); }

  Token expect(TokenType type) {
    if (current_.type != type) {
      error("expected " + token_name(type) + ", got '" + current_.value + "'");
      return {TokenType::kError, "", current_.line, current_.col};
    }
    Token tok = current_;
    advance();
    return tok;
  }

  void error(const String& msg) {
    errors_->push_back({msg, file_, current_.line, current_.col});
    // Recovery: skip to next '}' or EOF
    while (current_.type != TokenType::kRBrace &&
           current_.type != TokenType::kEof)
      advance();
  }

  static String token_name(TokenType type) {
    switch (type) {
      case TokenType::kIdentifier:
        return "identifier";
      case TokenType::kString:
        return "string";
      case TokenType::kNumber:
        return "number";
      case TokenType::kHexColor:
        return "hex color";
      case TokenType::kColon:
        return "':'";
      case TokenType::kSemicolon:
        return "';'";
      case TokenType::kLBrace:
        return "'{'";
      case TokenType::kRBrace:
        return "'}'";
      case TokenType::kAt:
        return "'@'";
      case TokenType::kEof:
        return "EOF";
      case TokenType::kError:
        return "error";
    }
    return "?";
  }

  // element = identifier [identifier] '{' (property | state_block | at_rule
  //           | element)* '}'
  UguiNode parse_element() {
    UguiNode node;
    node.source_line = current_.line;

    auto type_tok = expect(TokenType::kIdentifier);
    node.type = type_tok.value;

    // Optional name
    if (current_.type == TokenType::kIdentifier) {
      node.name = current_.value;
      advance();
    }

    return parse_element_body(ugui::move(node));
  }

  // The '{' ... '}' of an element, at any depth: children go through here
  // too, so @media and @keyframes work on nested widgets the same as on the
  // root.
  UguiNode parse_element_body(UguiNode node) {
    expect(TokenType::kLBrace);

    while (current_.type != TokenType::kRBrace &&
           current_.type != TokenType::kEof) {
      if (current_.type == TokenType::kAt) {
        advance();  // skip '@'
        if (current_.type == TokenType::kIdentifier &&
            current_.value == "media") {
          node.media_queries.push_back(parse_media_query());
        } else {
          node.keyframe_blocks.push_back(parse_keyframe_block_inner());
        }
      } else if (current_.type == TokenType::kColon) {
        // State block: :hover { ... }
        node.state_blocks.push_back(parse_state_block());
      } else if (current_.type == TokenType::kIdentifier) {
        // A property (identifier: value;) or a child element.
        Token id = current_;
        advance();
        if (current_.type == TokenType::kColon) {
          advance();  // skip ':'
          String value = parse_value();
          if (current_.type == TokenType::kSemicolon) advance();
          node.properties[id.value] = value;
        } else if (current_.type == TokenType::kLBrace ||
                   current_.type == TokenType::kIdentifier) {
          UguiNode child;
          child.source_line = id.line;
          child.type = id.value;
          if (current_.type == TokenType::kIdentifier) {
            child.name = current_.value;
            advance();
          }
          node.children.push_back(parse_element_body(ugui::move(child)));
        } else {
          error("unexpected token after identifier '" + id.value + "'");
        }
      } else {
        error("unexpected token '" + current_.value + "'");
        advance();
      }
    }

    if (current_.type == TokenType::kRBrace) advance();
    return node;
  }

  // state_block = ':' identifier '{' (property)* '}'
  UguiNode::StateBlock parse_state_block() {
    UguiNode::StateBlock sb;
    advance();  // skip ':'
    sb.state = current_.value;
    advance();  // state name
    expect(TokenType::kLBrace);

    while (current_.type != TokenType::kRBrace &&
           current_.type != TokenType::kEof) {
      if (current_.type == TokenType::kIdentifier) {
        String key = current_.value;
        advance();
        expect(TokenType::kColon);
        String val = parse_value();
        if (current_.type == TokenType::kSemicolon) advance();
        sb.properties[key] = val;
      } else {
        advance();
      }
    }
    if (current_.type == TokenType::kRBrace) advance();
    return sb;
  }

  // @keyframes name { properties; percent% { props } ... }
  // Called when '@' has already been consumed by the caller.
  UguiNode::KeyframeBlock parse_keyframe_block_inner() {
    UguiNode::KeyframeBlock kb;

    // Expect "keyframes" identifier (current_ should be it)
    if (current_.type != TokenType::kIdentifier ||
        current_.value != "keyframes") {
      error("expected 'keyframes' after '@'");
      return kb;
    }
    advance();  // skip "keyframes"

    // Animation name
    if (current_.type == TokenType::kIdentifier) {
      kb.name = current_.value;
      advance();
    }

    expect(TokenType::kLBrace);

    while (current_.type != TokenType::kRBrace &&
           current_.type != TokenType::kEof) {
      if (current_.type == TokenType::kNumber && current_.value.back() == '%') {
        // Percentage keyframe stop: e.g. 50% { opacity: 0.6; }
        UguiNode::KeyframeBlock::Stop stop;
        String pval = current_.value;
        pval.pop_back();  // remove '%'
        stop.percent = 0;
        ugui::from_chars(pval.data(), pval.data() + pval.size(), stop.percent);
        stop.percent /= 100.0f;
        advance();
        expect(TokenType::kLBrace);

        while (current_.type != TokenType::kRBrace &&
               current_.type != TokenType::kEof) {
          if (current_.type == TokenType::kIdentifier) {
            String key = current_.value;
            advance();
            expect(TokenType::kColon);
            String val = parse_value();
            if (current_.type == TokenType::kSemicolon) advance();
            stop.properties[key] = val;
          } else {
            advance();
          }
        }
        if (current_.type == TokenType::kRBrace) advance();
        kb.stops.push_back(ugui::move(stop));
      } else if (current_.type == TokenType::kIdentifier) {
        // Top-level property: duration, loop, alternate, easing
        String key = current_.value;
        advance();
        expect(TokenType::kColon);
        String val = parse_value();
        if (current_.type == TokenType::kSemicolon) advance();
        kb.properties[key] = val;
      } else {
        advance();
      }
    }
    if (current_.type == TokenType::kRBrace) advance();
    return kb;
  }

  // Legacy entry point: consumes '@' then delegates
  UguiNode::KeyframeBlock parse_keyframe_block() {
    advance();  // skip '@'
    return parse_keyframe_block_inner();
  }

  // @media (condition: value) { property: value; ... }
  // Called when '@' has already been consumed and current_ is "media".
  UguiNode::MediaQuery parse_media_query() {
    UguiNode::MediaQuery mq;
    advance();  // skip "media"

    // Tokens until '{'; '(' ')' are not lexer tokens (kError) and colons
    // inside the condition are kColon. Skip them.
    while (current_.type != TokenType::kLBrace &&
           current_.type != TokenType::kEof) {
      if (current_.type == TokenType::kIdentifier) {
        if (mq.condition.empty()) mq.condition = current_.value;
      } else if (current_.type == TokenType::kNumber) {
        f32 v = 0;
        ugui::from_chars(current_.value.data(),
                        current_.value.data() + current_.value.size(), v);
        mq.value = v;
      }
      // Skip kColon, kError ('(' and ')'), and anything else
      advance();
    }

    // Parse property overrides inside braces
    if (current_.type == TokenType::kLBrace) {
      advance();  // skip '{'
      while (current_.type != TokenType::kRBrace &&
             current_.type != TokenType::kEof) {
        if (current_.type == TokenType::kIdentifier) {
          String key = current_.value;
          advance();
          if (current_.type == TokenType::kColon) {
            advance();  // skip ':'
            String val = parse_value();
            if (current_.type == TokenType::kSemicolon) advance();
            mq.properties[key] = val;
          }
        } else {
          advance();
        }
      }
      if (current_.type == TokenType::kRBrace) advance();
    }

    return mq;
  }

  // value = (identifier | string | number | hex_color)+
  String parse_value() {
    String val;
    while (current_.type != TokenType::kSemicolon &&
           current_.type != TokenType::kRBrace &&
           current_.type != TokenType::kEof) {
      if (!val.empty()) val += ' ';
      val += current_.value;
      advance();
    }
    return val;
  }

  Lexer lexer_;
  Token current_;
  const char* file_;
  Vector<ParseError>* errors_ = nullptr;
};

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

// The std::filesystem operations import resolution needs (parent_path,
// operator/, weakly_canonical), spelled out so both container configurations
// resolve and report import paths identically. POSIX follows libstdc++
// exactly; Windows resolves through _fullpath, which normalizes and makes the
// path absolute but does not resolve symlinks.
namespace {

bool IsSeparator(char c) {
#if defined(_WIN32)
  return c == '/' || c == '\\';
#else
  return c == '/';
#endif
}

bool IsAbsolute(const String& p) {
#if defined(_WIN32)
  return p.size() >= 3 && p[1] == ':' && IsSeparator(p[2]);
#else
  return !p.empty() && p[0] == '/';
#endif
}

// Everything up to the end of the second-to-last element: "a/b/c" -> "a/b",
// "a/b/" -> "a/b", "/x" -> "/", "x" -> "", "/" -> "/".
String ParentPath(const String& p) {
  usize end = p.size();
  while (end > 0 && IsSeparator(p[end - 1])) --end;
  if (end == 0) return p;  // "" or only separators: no relative path
  if (end < p.size()) return p.substr(0, end);  // last element is the empty one
  while (end > 0 && !IsSeparator(p[end - 1])) --end;  // drop the filename
  if (end == 0) return String();
  usize keep = end;
  while (keep > 0 && IsSeparator(p[keep - 1])) --keep;
  return keep == 0 ? p.substr(0, 1) : p.substr(0, keep);
}

// operator/: an absolute rhs replaces, otherwise a separator is added unless
// lhs is empty or already ends in one.
String JoinPath(const String& lhs, const String& rhs) {
  if (IsAbsolute(rhs) || lhs.empty()) return rhs;
  String out = lhs;
  if (!IsSeparator(out[out.size() - 1])) out += '/';
  out += rhs;
  return out;
}

#if !defined(_WIN32)
// Splits into elements the way std::filesystem::path iterates: the root "/",
// each filename, and an empty trailing element for a trailing separator.
Vector<String> PathElements(const String& p) {
  Vector<String> elements;
  usize i = 0;
  if (!p.empty() && p[0] == '/') {
    elements.push_back(String("/"));
    while (i < p.size() && p[i] == '/') ++i;
  }
  while (i < p.size()) {
    usize start = i;
    while (i < p.size() && p[i] != '/') ++i;
    elements.push_back(p.substr(start, i - start));
    while (i < p.size() && p[i] == '/') ++i;
    if (i == p.size() && p[i - 1] == '/') elements.push_back(String());
  }
  return elements;
}

// std::filesystem::path::lexically_normal.
String LexicallyNormal(const String& p) {
  if (p.find_first_not_of('/') == String::npos) return p;  // "" or only roots
  Vector<String> elements = PathElements(p);
  const bool absolute = !elements.empty() && elements[0] == "/";
  Vector<String> out;
  bool trailing = false;
  for (usize i = absolute ? 1 : 0; i < elements.size(); ++i) {
    const String& e = elements[i];
    const bool last = i + 1 == elements.size();
    if (e.empty()) {
      trailing = !out.empty();
    } else if (e == ".") {
      trailing = last ? !out.empty() : trailing;
    } else if (e == "..") {
      if (!out.empty() && out[out.size() - 1] != "..") {
        out.pop_back();
        trailing = !out.empty();
      } else if (!absolute) {
        out.push_back(e);
        trailing = false;
      }
    } else {
      out.push_back(e);
      trailing = false;
    }
  }
  String result = absolute ? String("/") : String();
  for (usize i = 0; i < out.size(); ++i) {
    if (i > 0) result += '/';
    result += out[i];
  }
  if (trailing && !out.empty() && out[out.size() - 1] != "..") result += '/';
  if (result.empty()) result = ".";
  return result;
}
#endif

// std::filesystem::weakly_canonical: the longest existing prefix made
// canonical, the rest appended and lexically normalized. Returns false where
// std reports an error (the caller then falls back to the plain path).
bool WeaklyCanonical(const String& p, String& out) {
#if defined(_WIN32)
  char* full = _fullpath(nullptr, p.c_str(), 0);
  if (!full) return false;
  out = full;
  free(full);
  return true;
#else
  Vector<String> elements = PathElements(p);
  String result;
  usize i = 0;
  for (; i < elements.size(); ++i) {
    String candidate = JoinPath(result, elements[i]);
    struct stat st;
    if (stat(candidate.c_str(), &st) != 0) {
      if (errno != ENOENT && errno != ENOTDIR) return false;
      break;
    }
    result = candidate;
  }
  if (!result.empty()) {
    char* real = realpath(result.c_str(), nullptr);
    if (!real) return false;
    result = real;
    free(real);
  }
  for (; i < elements.size(); ++i) result = JoinPath(result, elements[i]);
  out = LexicallyNormal(result);
  return true;
#endif
}

}  // namespace

// ---------------------------------------------------------------------------
// Import resolution
// ---------------------------------------------------------------------------

static bool ParseUguiFileInner(const char* path, UguiDocument& out_doc,
                               Vector<ParseError>& out_errors,
                               Vector<String>& visited);

// Merge each imported file's components and style classes into `doc` before
// its own definitions, so the importer wins on name clashes. Root widgets of
// imports are ignored. `visited` holds canonical paths on the import chain to
// break cycles.
static void ResolveImports(UguiDocument& doc, Vector<ParseError>& errors,
                           Vector<String>& visited) {
  if (doc.imports.empty()) return;

  String base = ParentPath(doc.source_path);

  Vector<UguiDocument::Component> imported_components;
  Vector<UguiDocument::StyleClass> imported_classes;

  for (auto& import_path : doc.imports) {
    String joined = JoinPath(base, import_path);
    String key;
    if (!WeaklyCanonical(joined, key)) key = joined;

    bool seen = false;
    for (auto& v : visited)
      if (v == key) {
        seen = true;
        break;
      }
    if (seen) continue;  // already imported somewhere up the chain
    visited.push_back(key);

    UguiDocument sub;
    ParseUguiFileInner(key.c_str(), sub, errors, visited);
    for (auto& c : sub.components) imported_components.push_back(ugui::move(c));
    for (auto& sc : sub.style_classes)
      imported_classes.push_back(ugui::move(sc));
  }

  for (auto& c : doc.components) imported_components.push_back(ugui::move(c));
  doc.components = ugui::move(imported_components);

  for (auto& sc : doc.style_classes) imported_classes.push_back(ugui::move(sc));
  doc.style_classes = ugui::move(imported_classes);
}

static bool ParseUguiFileInner(const char* path, UguiDocument& out_doc,
                               Vector<ParseError>& out_errors,
                               Vector<String>& visited) {
  // Text mode, as the ifstream this replaced: the buffer is sized to the file
  // and a shorter read (CRLF folding on Windows) leaves NULs at the end.
  FILE* file = fopen(path, "r");
  if (!file) {
    out_errors.push_back({"failed to open file", path, 0, 0});
    return false;
  }

  fseek(file, 0, SEEK_END);
  auto size = static_cast<usize>(ftell(file));
  fseek(file, 0, SEEK_SET);
  String buffer(size, '\0');
  size_t read = fread(buffer.data(), 1, size, file);
  (void)read;
  fclose(file);

  Parser parser(buffer.c_str(), buffer.size(), path);
  bool ok = parser.parse(out_doc, out_errors);
  ResolveImports(out_doc, out_errors, visited);
  return ok && out_errors.empty();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool ParseUgui(const char* source, usize source_len, const char* filename,
               UguiDocument& out_doc, Vector<ParseError>& out_errors) {
  Parser parser(source, source_len, filename);
  bool ok = parser.parse(out_doc, out_errors);
  Vector<String> visited;
  ResolveImports(out_doc, out_errors, visited);
  return ok && out_errors.empty();
}

bool ParseUguiFile(const char* path, UguiDocument& out_doc,
                   Vector<ParseError>& out_errors) {
  Vector<String> visited;
  String canonical;
  visited.push_back(WeaklyCanonical(path, canonical) ? canonical
                                                     : String(path));
  return ParseUguiFileInner(path, out_doc, out_errors, visited);
}

}  // namespace ugui
