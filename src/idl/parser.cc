#include <ultragui/idl/parser.h>

#include <charconv>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace ugui {

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------

enum class TokenType : u8 {
    kIdentifier,
    kString, // "quoted string"
    kNumber,
    kHexColor, // #RRGGBB or #RRGGBBAA
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

        if (pos_ >= len_)
            return {TokenType::kEof, "", line_, col_};

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

        if (c == '"')
            return lex_string(tok_line, tok_col);

        if (c == '#')
            return lex_hex_color(tok_line, tok_col);

        if (std::isdigit(c) || (c == '-' && pos_ + 1 < len_ && std::isdigit(src_[pos_ + 1])) ||
            c == '.')
            return lex_number(tok_line, tok_col);

        if (std::isalpha(c) || c == '_' || c == '-')
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
            } else if (pos_ + 1 < len_ && src_[pos_] == '/' && src_[pos_ + 1] == '/') {
                while (pos_ < len_ && src_[pos_] != '\n')
                    advance();
            } else if (pos_ + 1 < len_ && src_[pos_] == '/' && src_[pos_ + 1] == '*') {
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
        advance(); // skip opening "
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
        if (pos_ < len_)
            advance(); // skip closing "
        return {TokenType::kString, val, line, col};
    }

    Token lex_hex_color(u32 line, u32 col) {
        advance(); // skip #
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
            } else if (pos_ + 1 < len_ && src_[pos_] == 'p' && src_[pos_ + 1] == 'x') {
                val += "px";
                advance();
                advance();
            } else if (pos_ + 1 < len_ && src_[pos_] == 'v' && src_[pos_ + 1] == 'w') {
                val += "vw";
                advance();
                advance();
            } else if (pos_ + 1 < len_ && src_[pos_] == 'v' && src_[pos_ + 1] == 'h') {
                val += "vh";
                advance();
                advance();
            } else if (pos_ + 1 < len_ && src_[pos_] == 'm' && src_[pos_ + 1] == 's') {
                val += "ms";
                advance();
                advance();
            } else if (src_[pos_] == 's' && (pos_ + 1 >= len_ || !std::isalpha(src_[pos_ + 1]))) {
                val += "s";
                advance();
            } else if (pos_ + 1 < len_ && src_[pos_] == 'f' && src_[pos_ + 1] == 'r') {
                val += "fr";
                advance();
                advance();
            }
        }
        return {TokenType::kNumber, val, line, col};
    }

    Token lex_identifier(u32 line, u32 col) {
        String val;
        while (pos_ < len_ &&
               (std::isalnum(src_[pos_]) || src_[pos_] == '_' || src_[pos_] == '-')) {
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
    Parser(const char* src, usize len, const char* file) : lexer_(src, len, file), file_(file) {
        advance();
    }

    bool parse(UguiDocument& doc, Vector<ParseError>& errors) {
        errors_ = &errors;
        doc.source_path = file_;

        while (current_.type != TokenType::kEof) {
            auto node = parse_element();
            if (node.type.empty())
                break;
            doc.roots.push_back(std::move(node));
        }

        return errors.empty();
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
        while (current_.type != TokenType::kRBrace && current_.type != TokenType::kEof)
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

    // element = identifier [identifier] '{' (property | state_block | element)* '}'
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

        expect(TokenType::kLBrace);

        while (current_.type != TokenType::kRBrace && current_.type != TokenType::kEof) {
            if (current_.type == TokenType::kAt) {
                // Peek at the next token to distinguish @media from @keyframes
                Token at_tok = current_;
                advance(); // skip '@'
                if (current_.type == TokenType::kIdentifier && current_.value == "media") {
                    auto mq = parse_media_query();
                    node.media_queries.push_back(std::move(mq));
                } else {
                    // Put back: parse_keyframe_block expects current_ to be '@'
                    // We already consumed '@', so call the inner keyframe logic directly
                    auto kb = parse_keyframe_block_inner();
                    node.keyframe_blocks.push_back(std::move(kb));
                }
            } else if (current_.type == TokenType::kColon) {
                // State block: :hover { ... }
                auto sb = parse_state_block();
                node.state_blocks.push_back(std::move(sb));
            } else if (current_.type == TokenType::kIdentifier) {
                // Peek ahead: is this a property (identifier: value;) or a child element?
                Token id = current_;
                advance();
                if (current_.type == TokenType::kColon) {
                    // Property
                    advance(); // skip ':'
                    String value = parse_value();
                    if (current_.type == TokenType::kSemicolon)
                        advance();
                    node.properties[id.value] = value;
                } else if (current_.type == TokenType::kLBrace ||
                           current_.type == TokenType::kIdentifier) {
                    // Child element (push back the identifier)
                    UguiNode child;
                    child.source_line = id.line;
                    child.type = id.value;
                    if (current_.type == TokenType::kIdentifier) {
                        child.name = current_.value;
                        advance();
                    }
                    expect(TokenType::kLBrace);
                    // Parse child body
                    while (current_.type != TokenType::kRBrace && current_.type != TokenType::kEof) {
                        if (current_.type == TokenType::kColon) {
                            child.state_blocks.push_back(parse_state_block());
                        } else if (current_.type == TokenType::kIdentifier) {
                            Token cid = current_;
                            advance();
                            if (current_.type == TokenType::kColon) {
                                advance();
                                String val = parse_value();
                                if (current_.type == TokenType::kSemicolon)
                                    advance();
                                child.properties[cid.value] = val;
                            } else {
                                // Nested child
                                UguiNode nested;
                                nested.source_line = cid.line;
                                nested.type = cid.value;
                                if (current_.type == TokenType::kIdentifier) {
                                    nested.name = current_.value;
                                    advance();
                                }
                                if (current_.type == TokenType::kLBrace) {
                                    // Recursively parse (simplified - only 3 levels deep here)
                                    nested = parse_element_body(nested);
                                }
                                child.children.push_back(std::move(nested));
                            }
                        } else {
                            advance(); // skip unexpected tokens
                        }
                    }
                    if (current_.type == TokenType::kRBrace)
                        advance();
                    node.children.push_back(std::move(child));
                } else {
                    error("unexpected token after identifier '" + id.value + "'");
                }
            } else {
                error("unexpected token '" + current_.value + "'");
                advance();
            }
        }

        if (current_.type == TokenType::kRBrace)
            advance();

        return node;
    }

    UguiNode parse_element_body(UguiNode node) {
        expect(TokenType::kLBrace);
        while (current_.type != TokenType::kRBrace && current_.type != TokenType::kEof) {
            if (current_.type == TokenType::kColon) {
                node.state_blocks.push_back(parse_state_block());
            } else if (current_.type == TokenType::kIdentifier) {
                Token id = current_;
                advance();
                if (current_.type == TokenType::kColon) {
                    advance();
                    String val = parse_value();
                    if (current_.type == TokenType::kSemicolon)
                        advance();
                    node.properties[id.value] = val;
                } else {
                    // Nested element
                    UguiNode child;
                    child.source_line = id.line;
                    child.type = id.value;
                    if (current_.type == TokenType::kIdentifier) {
                        child.name = current_.value;
                        advance();
                    }
                    if (current_.type == TokenType::kLBrace) {
                        child = parse_element_body(child);
                    }
                    node.children.push_back(std::move(child));
                }
            } else {
                advance();
            }
        }
        if (current_.type == TokenType::kRBrace)
            advance();
        return node;
    }

    // state_block = ':' identifier '{' (property)* '}'
    UguiNode::StateBlock parse_state_block() {
        UguiNode::StateBlock sb;
        advance(); // skip ':'
        sb.state = current_.value;
        advance(); // state name
        expect(TokenType::kLBrace);

        while (current_.type != TokenType::kRBrace && current_.type != TokenType::kEof) {
            if (current_.type == TokenType::kIdentifier) {
                String key = current_.value;
                advance();
                expect(TokenType::kColon);
                String val = parse_value();
                if (current_.type == TokenType::kSemicolon)
                    advance();
                sb.properties[key] = val;
            } else {
                advance();
            }
        }
        if (current_.type == TokenType::kRBrace)
            advance();
        return sb;
    }

    // @keyframes name { properties; percent% { props } ... }
    // Called when '@' has already been consumed by the caller.
    UguiNode::KeyframeBlock parse_keyframe_block_inner() {
        UguiNode::KeyframeBlock kb;

        // Expect "keyframes" identifier (current_ should be it)
        if (current_.type != TokenType::kIdentifier || current_.value != "keyframes") {
            error("expected 'keyframes' after '@'");
            return kb;
        }
        advance(); // skip "keyframes"

        // Animation name
        if (current_.type == TokenType::kIdentifier) {
            kb.name = current_.value;
            advance();
        }

        expect(TokenType::kLBrace);

        while (current_.type != TokenType::kRBrace && current_.type != TokenType::kEof) {
            if (current_.type == TokenType::kNumber && current_.value.back() == '%') {
                // Percentage keyframe stop: e.g. 50% { opacity: 0.6; }
                UguiNode::KeyframeBlock::Stop stop;
                String pval = current_.value;
                pval.pop_back(); // remove '%'
                stop.percent = 0;
                std::from_chars(pval.data(), pval.data() + pval.size(), stop.percent);
                stop.percent /= 100.0f;
                advance();
                expect(TokenType::kLBrace);

                while (current_.type != TokenType::kRBrace && current_.type != TokenType::kEof) {
                    if (current_.type == TokenType::kIdentifier) {
                        String key = current_.value;
                        advance();
                        expect(TokenType::kColon);
                        String val = parse_value();
                        if (current_.type == TokenType::kSemicolon)
                            advance();
                        stop.properties[key] = val;
                    } else {
                        advance();
                    }
                }
                if (current_.type == TokenType::kRBrace)
                    advance();
                kb.stops.push_back(std::move(stop));
            } else if (current_.type == TokenType::kIdentifier) {
                // Top-level property: duration, loop, alternate, easing
                String key = current_.value;
                advance();
                expect(TokenType::kColon);
                String val = parse_value();
                if (current_.type == TokenType::kSemicolon)
                    advance();
                kb.properties[key] = val;
            } else {
                advance();
            }
        }
        if (current_.type == TokenType::kRBrace)
            advance();
        return kb;
    }

    // Legacy entry point: consumes '@' then delegates
    UguiNode::KeyframeBlock parse_keyframe_block() {
        advance(); // skip '@'
        return parse_keyframe_block_inner();
    }

    // @media (condition: value) { property: value; ... }
    // Called when '@' has already been consumed and current_ is "media".
    UguiNode::MediaQuery parse_media_query() {
        UguiNode::MediaQuery mq;
        advance(); // skip "media"

        // Parse tokens until '{', extracting the condition identifier and numeric value.
        // Parentheses '(' and ')' are not lexer tokens - they appear as kError tokens;
        // colons inside the condition also appear as kColon. Skip them gracefully.
        while (current_.type != TokenType::kLBrace && current_.type != TokenType::kEof) {
            if (current_.type == TokenType::kIdentifier) {
                if (mq.condition.empty())
                    mq.condition = current_.value;
            } else if (current_.type == TokenType::kNumber) {
                f32 v = 0;
                std::from_chars(current_.value.data(),
                                current_.value.data() + current_.value.size(), v);
                mq.value = v;
            }
            // Skip kColon, kError ('(' and ')'), and anything else
            advance();
        }

        // Parse property overrides inside braces
        if (current_.type == TokenType::kLBrace) {
            advance(); // skip '{'
            while (current_.type != TokenType::kRBrace && current_.type != TokenType::kEof) {
                if (current_.type == TokenType::kIdentifier) {
                    String key = current_.value;
                    advance();
                    if (current_.type == TokenType::kColon) {
                        advance(); // skip ':'
                        String val = parse_value();
                        if (current_.type == TokenType::kSemicolon)
                            advance();
                        mq.properties[key] = val;
                    }
                } else {
                    advance();
                }
            }
            if (current_.type == TokenType::kRBrace)
                advance();
        }

        return mq;
    }

    // value = (identifier | string | number | hex_color)+
    String parse_value() {
        String val;
        while (current_.type != TokenType::kSemicolon && current_.type != TokenType::kRBrace &&
               current_.type != TokenType::kEof) {
            if (!val.empty())
                val += ' ';
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
// Public API
// ---------------------------------------------------------------------------

bool ParseUgui(const char* source, usize source_len, const char* filename, UguiDocument& out_doc,
                Vector<ParseError>& out_errors) {
    Parser parser(source, source_len, filename);
    return parser.parse(out_doc, out_errors);
}

bool ParseUguiFile(const char* path, UguiDocument& out_doc, Vector<ParseError>& out_errors) {
    std::ifstream file(path, std::ios::ate);
    if (!file.is_open()) {
        out_errors.push_back({"failed to open file", path, 0, 0});
        return false;
    }

    auto size = static_cast<usize>(file.tellg());
    String buffer(size, '\0');
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));

    return ParseUgui(buffer.c_str(), buffer.size(), path, out_doc, out_errors);
}

} // namespace ugui
