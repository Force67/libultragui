#include <ugui/idl/parser.h>
#include <ugui/core/from_chars_compat.h>

#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

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

    if (std::isdigit(c) ||
        (c == '-' && pos_ + 1 < len_ && std::isdigit(src_[pos_ + 1])) ||
        c == '.')
      return lex_number(tok_line, tok_col);

    if (std::isalpha(c) || c == '_' || c == '-' || c == '$')
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
      if (std::isspace(src_[pos_])) {
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
    while (pos_ < len_ && std::isxdigit(src_[pos_])) {
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
    while (pos_ < len_ && (std::isdigit(src_[pos_]) || src_[pos_] == '.')) {
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
                 (pos_ + 1 >= len_ || !std::isalpha(src_[pos_ + 1]))) {
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
    while (pos_ < len_ && (std::isalnum(src_[pos_]) || src_[pos_] == '_' ||
                           src_[pos_] == '-')) {
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
        if (!sc.name.empty()) doc.style_classes.push_back(std::move(sc));
        continue;
      }
      if (current_.type == TokenType::kIdentifier &&
          current_.value == "component") {
        auto comp = parse_component();
        if (!comp.name.empty()) doc.components.push_back(std::move(comp));
        continue;
      }
      if (current_.type == TokenType::kIdentifier &&
          current_.value == "import") {
        String path = parse_import();
        if (!path.empty()) doc.imports.push_back(std::move(path));
        continue;
      }
      auto node = parse_element();
      if (node.type.empty()) break;
      doc.roots.push_back(std::move(node));
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
          comp.root = std::move(el);
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

    return parse_element_body(std::move(node));
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
          node.children.push_back(parse_element_body(std::move(child)));
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
        kb.stops.push_back(std::move(stop));
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

  namespace fs = std::filesystem;
  fs::path base = fs::path(doc.source_path.c_str()).parent_path();

  Vector<UguiDocument::Component> imported_components;
  Vector<UguiDocument::StyleClass> imported_classes;

  for (auto& import_path : doc.imports) {
    std::error_code ec;
    fs::path resolved = fs::weakly_canonical(base / import_path.c_str(), ec);
    String key = ec ? String((base / import_path.c_str()).string())
                    : String(resolved.string());

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
    for (auto& c : sub.components)
      imported_components.push_back(std::move(c));
    for (auto& sc : sub.style_classes)
      imported_classes.push_back(std::move(sc));
  }

  imported_components.insert(imported_components.end(),
                             std::make_move_iterator(doc.components.begin()),
                             std::make_move_iterator(doc.components.end()));
  doc.components = std::move(imported_components);

  imported_classes.insert(imported_classes.end(),
                          std::make_move_iterator(doc.style_classes.begin()),
                          std::make_move_iterator(doc.style_classes.end()));
  doc.style_classes = std::move(imported_classes);
}

static bool ParseUguiFileInner(const char* path, UguiDocument& out_doc,
                               Vector<ParseError>& out_errors,
                               Vector<String>& visited) {
  std::ifstream file(path, std::ios::ate);
  if (!file.is_open()) {
    out_errors.push_back({"failed to open file", path, 0, 0});
    return false;
  }

  auto size = static_cast<usize>(file.tellg());
  String buffer(size, '\0');
  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(size));

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
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path canonical = fs::weakly_canonical(path, ec);
  visited.push_back(ec ? String(path) : String(canonical.string()));
  return ParseUguiFileInner(path, out_doc, out_errors, visited);
}

}  // namespace ugui
