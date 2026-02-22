#ifndef ULTRAGUI_IDL_PARSER_H_
#define ULTRAGUI_IDL_PARSER_H_

#include <ultragui/core/types.h>

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
    String type; // "panel", "button", "text", "image", etc.
    String name; // User-defined identifier
    HashMap<String, String> properties;

    struct StateBlock {
        String state; // "hover", "pressed", "focused", etc.
        HashMap<String, String> properties;
    };
    Vector<StateBlock> state_blocks;

    struct KeyframeBlock {
        String name;
        HashMap<String, String> properties; // duration, loop, alternate, easing
        struct Stop {
            f32 percent; // 0.0-1.0
            HashMap<String, String> properties;
        };
        Vector<Stop> stops;
    };
    Vector<KeyframeBlock> keyframe_blocks;

    struct MediaQuery {
        String condition; // e.g., "min-width", "max-width", "min-height", "max-height"
        f32 value = 0.0f;     // e.g., 800
        HashMap<String, String> properties; // style overrides when condition matches
    };
    Vector<MediaQuery> media_queries;

    Vector<UguiNode> children;

    u32 source_line = 0;
};

/// Parsed .ugui document
struct UguiDocument {
    Vector<UguiNode> roots;
    String source_path;
};

/// Parse error info
struct ParseError {
    String message;
    String file;
    u32 line = 0;
    u32 column = 0;
};

/// Parse a .ugui file from source text.
/// Returns true on success; on failure, errors are populated.
bool ParseUgui(const char* source, usize source_len, const char* filename, UguiDocument& out_doc,
                Vector<ParseError>& out_errors);

/// Parse from file path.
bool ParseUguiFile(const char* path, UguiDocument& out_doc, Vector<ParseError>& out_errors);

} // namespace ugui

#endif  // ULTRAGUI_IDL_PARSER_H_
