#ifndef ULTRAGUI_IDL_PARSER_H_
#define ULTRAGUI_IDL_PARSER_H_

#include <ultragui/core/types.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace ugui {

/// AST node representing an element in a .ugui file.
///
/// Example:
///   panel main_menu {
///     layout: column;
///     background: #1a1a2e;
///
///     button btn_play {
///       text: "Play";
///       :hover { background: #e94560; }
///     }
///   }
///
struct UguiNode {
    std::string type; // "panel", "button", "text", "image", etc.
    std::string name; // User-defined identifier
    std::unordered_map<std::string, std::string> properties;

    struct StateBlock {
        std::string state; // "hover", "pressed", "focused", etc.
        std::unordered_map<std::string, std::string> properties;
    };
    std::vector<StateBlock> state_blocks;

    struct KeyframeBlock {
        std::string name;
        std::unordered_map<std::string, std::string> properties; // duration, loop, alternate, easing
        struct Stop {
            f32 percent; // 0.0-1.0
            std::unordered_map<std::string, std::string> properties;
        };
        std::vector<Stop> stops;
    };
    std::vector<KeyframeBlock> keyframe_blocks;

    std::vector<UguiNode> children;

    u32 source_line = 0;
};

/// Parsed .ugui document
struct UguiDocument {
    std::vector<UguiNode> roots;
    std::string source_path;
};

/// Parse error info
struct ParseError {
    std::string message;
    std::string file;
    u32 line = 0;
    u32 column = 0;
};

/// Parse a .ugui file from source text.
/// Returns true on success; on failure, errors are populated.
bool ParseUgui(const char* source, usize source_len, const char* filename, UguiDocument& out_doc,
                std::vector<ParseError>& out_errors);

/// Parse from file path.
bool ParseUguiFile(const char* path, UguiDocument& out_doc, std::vector<ParseError>& out_errors);

} // namespace ugui

#endif  // ULTRAGUI_IDL_PARSER_H_
