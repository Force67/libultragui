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
    Identifier,
    String, // "quoted string"
    Number,
    HexColor, // #RRGGBB or #RRGGBBAA
    Colon,
    Semicolon,
    LBrace,
    RBrace,
    At,  // @
    Eof,
    Error,
};

struct Token {
    TokenType type;
    std::string value;
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
            return {TokenType::Eof, "", line_, col_};

        char c = src_[pos_];
        u32 tok_line = line_, tok_col = col_;

        if (c == '{') {
            advance();
            return {TokenType::LBrace, "{", tok_line, tok_col};
        }
        if (c == '}') {
            advance();
            return {TokenType::RBrace, "}", tok_line, tok_col};
        }
        if (c == ':') {
            advance();
            return {TokenType::Colon, ":", tok_line, tok_col};
        }
        if (c == ';') {
            advance();
            return {TokenType::Semicolon, ";", tok_line, tok_col};
        }
        if (c == '@') {
            advance();
            return {TokenType::At, "@", tok_line, tok_col};
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
        return {TokenType::Error, std::string(1, c), tok_line, tok_col};
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
        std::string val;
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
        return {TokenType::String, val, line, col};
    }

    Token lex_hex_color(u32 line, u32 col) {
        advance(); // skip #
        std::string val = "#";
        while (pos_ < len_ && std::isxdigit(src_[pos_])) {
            val += src_[pos_];
            advance();
        }
        return {TokenType::HexColor, val, line, col};
    }

    Token lex_number(u32 line, u32 col) {
        std::string val;
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
        return {TokenType::Number, val, line, col};
    }

    Token lex_identifier(u32 line, u32 col) {
        std::string val;
        while (pos_ < len_ &&
               (std::isalnum(src_[pos_]) || src_[pos_] == '_' || src_[pos_] == '-')) {
            val += src_[pos_];
            advance();
        }
        return {TokenType::Identifier, val, line, col};
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

    bool parse(UguiDocument& doc, std::vector<ParseError>& errors) {
        errors_ = &errors;
        doc.source_path = file_;

        while (current_.type != TokenType::Eof) {
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
            return {TokenType::Error, "", current_.line, current_.col};
        }
        Token tok = current_;
        advance();
        return tok;
    }

    void error(const std::string& msg) {
        errors_->push_back({msg, file_, current_.line, current_.col});
        // Recovery: skip to next '}' or EOF
        while (current_.type != TokenType::RBrace && current_.type != TokenType::Eof)
            advance();
    }

    static std::string token_name(TokenType type) {
        switch (type) {
        case TokenType::Identifier:
            return "identifier";
        case TokenType::String:
            return "string";
        case TokenType::Number:
            return "number";
        case TokenType::HexColor:
            return "hex color";
        case TokenType::Colon:
            return "':'";
        case TokenType::Semicolon:
            return "';'";
        case TokenType::LBrace:
            return "'{'";
        case TokenType::RBrace:
            return "'}'";
        case TokenType::At:
            return "'@'";
        case TokenType::Eof:
            return "EOF";
        case TokenType::Error:
            return "error";
        }
        return "?";
    }

    // element = identifier [identifier] '{' (property | state_block | element)* '}'
    UguiNode parse_element() {
        UguiNode node;
        node.source_line = current_.line;

        auto type_tok = expect(TokenType::Identifier);
        node.type = type_tok.value;

        // Optional name
        if (current_.type == TokenType::Identifier) {
            node.name = current_.value;
            advance();
        }

        expect(TokenType::LBrace);

        while (current_.type != TokenType::RBrace && current_.type != TokenType::Eof) {
            if (current_.type == TokenType::At) {
                // @keyframes block
                auto kb = parse_keyframe_block();
                node.keyframe_blocks.push_back(std::move(kb));
            } else if (current_.type == TokenType::Colon) {
                // State block: :hover { ... }
                auto sb = parse_state_block();
                node.state_blocks.push_back(std::move(sb));
            } else if (current_.type == TokenType::Identifier) {
                // Peek ahead: is this a property (identifier: value;) or a child element?
                Token id = current_;
                advance();
                if (current_.type == TokenType::Colon) {
                    // Property
                    advance(); // skip ':'
                    std::string value = parse_value();
                    if (current_.type == TokenType::Semicolon)
                        advance();
                    node.properties[id.value] = value;
                } else if (current_.type == TokenType::LBrace ||
                           current_.type == TokenType::Identifier) {
                    // Child element (push back the identifier)
                    UguiNode child;
                    child.source_line = id.line;
                    child.type = id.value;
                    if (current_.type == TokenType::Identifier) {
                        child.name = current_.value;
                        advance();
                    }
                    expect(TokenType::LBrace);
                    // Parse child body
                    while (current_.type != TokenType::RBrace && current_.type != TokenType::Eof) {
                        if (current_.type == TokenType::Colon) {
                            child.state_blocks.push_back(parse_state_block());
                        } else if (current_.type == TokenType::Identifier) {
                            Token cid = current_;
                            advance();
                            if (current_.type == TokenType::Colon) {
                                advance();
                                std::string val = parse_value();
                                if (current_.type == TokenType::Semicolon)
                                    advance();
                                child.properties[cid.value] = val;
                            } else {
                                // Nested child
                                UguiNode nested;
                                nested.source_line = cid.line;
                                nested.type = cid.value;
                                if (current_.type == TokenType::Identifier) {
                                    nested.name = current_.value;
                                    advance();
                                }
                                if (current_.type == TokenType::LBrace) {
                                    // Recursively parse (simplified - only 3 levels deep here)
                                    nested = parse_element_body(nested);
                                }
                                child.children.push_back(std::move(nested));
                            }
                        } else {
                            advance(); // skip unexpected tokens
                        }
                    }
                    if (current_.type == TokenType::RBrace)
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

        if (current_.type == TokenType::RBrace)
            advance();

        return node;
    }

    UguiNode parse_element_body(UguiNode node) {
        expect(TokenType::LBrace);
        while (current_.type != TokenType::RBrace && current_.type != TokenType::Eof) {
            if (current_.type == TokenType::Colon) {
                node.state_blocks.push_back(parse_state_block());
            } else if (current_.type == TokenType::Identifier) {
                Token id = current_;
                advance();
                if (current_.type == TokenType::Colon) {
                    advance();
                    std::string val = parse_value();
                    if (current_.type == TokenType::Semicolon)
                        advance();
                    node.properties[id.value] = val;
                } else {
                    // Nested element
                    UguiNode child;
                    child.source_line = id.line;
                    child.type = id.value;
                    if (current_.type == TokenType::Identifier) {
                        child.name = current_.value;
                        advance();
                    }
                    if (current_.type == TokenType::LBrace) {
                        child = parse_element_body(child);
                    }
                    node.children.push_back(std::move(child));
                }
            } else {
                advance();
            }
        }
        if (current_.type == TokenType::RBrace)
            advance();
        return node;
    }

    // state_block = ':' identifier '{' (property)* '}'
    UguiNode::StateBlock parse_state_block() {
        UguiNode::StateBlock sb;
        advance(); // skip ':'
        sb.state = current_.value;
        advance(); // state name
        expect(TokenType::LBrace);

        while (current_.type != TokenType::RBrace && current_.type != TokenType::Eof) {
            if (current_.type == TokenType::Identifier) {
                std::string key = current_.value;
                advance();
                expect(TokenType::Colon);
                std::string val = parse_value();
                if (current_.type == TokenType::Semicolon)
                    advance();
                sb.properties[key] = val;
            } else {
                advance();
            }
        }
        if (current_.type == TokenType::RBrace)
            advance();
        return sb;
    }

    // @keyframes name { properties; percent% { props } ... }
    UguiNode::KeyframeBlock parse_keyframe_block() {
        UguiNode::KeyframeBlock kb;
        advance(); // skip '@'

        // Expect "keyframes" identifier
        if (current_.type != TokenType::Identifier || current_.value != "keyframes") {
            error("expected 'keyframes' after '@'");
            return kb;
        }
        advance(); // skip "keyframes"

        // Animation name
        if (current_.type == TokenType::Identifier) {
            kb.name = current_.value;
            advance();
        }

        expect(TokenType::LBrace);

        while (current_.type != TokenType::RBrace && current_.type != TokenType::Eof) {
            if (current_.type == TokenType::Number && current_.value.back() == '%') {
                // Percentage keyframe stop: e.g. 50% { opacity: 0.6; }
                UguiNode::KeyframeBlock::Stop stop;
                std::string pval = current_.value;
                pval.pop_back(); // remove '%'
                stop.percent = 0;
                std::from_chars(pval.data(), pval.data() + pval.size(), stop.percent);
                stop.percent /= 100.0f;
                advance();
                expect(TokenType::LBrace);

                while (current_.type != TokenType::RBrace && current_.type != TokenType::Eof) {
                    if (current_.type == TokenType::Identifier) {
                        std::string key = current_.value;
                        advance();
                        expect(TokenType::Colon);
                        std::string val = parse_value();
                        if (current_.type == TokenType::Semicolon)
                            advance();
                        stop.properties[key] = val;
                    } else {
                        advance();
                    }
                }
                if (current_.type == TokenType::RBrace)
                    advance();
                kb.stops.push_back(std::move(stop));
            } else if (current_.type == TokenType::Identifier) {
                // Top-level property: duration, loop, alternate, easing
                std::string key = current_.value;
                advance();
                expect(TokenType::Colon);
                std::string val = parse_value();
                if (current_.type == TokenType::Semicolon)
                    advance();
                kb.properties[key] = val;
            } else {
                advance();
            }
        }
        if (current_.type == TokenType::RBrace)
            advance();
        return kb;
    }

    // value = (identifier | string | number | hex_color)+
    std::string parse_value() {
        std::string val;
        while (current_.type != TokenType::Semicolon && current_.type != TokenType::RBrace &&
               current_.type != TokenType::Eof) {
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
    std::vector<ParseError>* errors_ = nullptr;
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool parse_ugui(const char* source, usize source_len, const char* filename, UguiDocument& out_doc,
                std::vector<ParseError>& out_errors) {
    Parser parser(source, source_len, filename);
    return parser.parse(out_doc, out_errors);
}

bool parse_ugui_file(const char* path, UguiDocument& out_doc, std::vector<ParseError>& out_errors) {
    std::ifstream file(path, std::ios::ate);
    if (!file.is_open()) {
        out_errors.push_back({"failed to open file", path, 0, 0});
        return false;
    }

    auto size = static_cast<usize>(file.tellg());
    std::string buffer(size, '\0');
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));

    return parse_ugui(buffer.c_str(), buffer.size(), path, out_doc, out_errors);
}

} // namespace ugui
