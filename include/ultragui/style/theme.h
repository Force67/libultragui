#ifndef ULTRAGUI_STYLE_THEME_H_
#define ULTRAGUI_STYLE_THEME_H_

#include <string>
#include <unordered_map>

namespace ugui {

/// A theme is a named set of CSS custom properties (design tokens).
struct Theme {
  std::string name;
  std::unordered_map<std::string, std::string> tokens;

  /// Set a token value (e.g., "--bg-primary", "#1a1a2e").
  void Set(const std::string& token_name, const std::string& value) {
    tokens[token_name] = value;
  }

  /// Get a token value, or empty string if not found.
  const std::string& Get(const std::string& token_name) const {
    static const std::string empty;
    auto it = tokens.find(token_name);
    return it != tokens.end() ? it->second : empty;
  }

  /// Dark theme preset with common design tokens.
  static Theme Dark() {
    Theme t;
    t.name = "dark";
    t.Set("--bg-primary", "#0f0f1a");
    t.Set("--bg-secondary", "#1a1a2e");
    t.Set("--bg-tertiary", "#2a2a45");
    t.Set("--bg-surface", "#1e1e35");
    t.Set("--text-primary", "#e0e0ff");
    t.Set("--text-secondary", "#8888aa");
    t.Set("--text-muted", "#555570");
    t.Set("--accent", "#4a4aff");
    t.Set("--accent-hover", "#5a5aff");
    t.Set("--accent-active", "#3a3aee");
    t.Set("--success", "#4aea8a");
    t.Set("--warning", "#ffaa33");
    t.Set("--error", "#ff4466");
    t.Set("--border", "#ffffff15");
    t.Set("--border-focus", "#4a4aff80");
    t.Set("--shadow", "#00000040");
    return t;
  }

  /// Light theme preset with common design tokens.
  static Theme Light() {
    Theme t;
    t.name = "light";
    t.Set("--bg-primary", "#ffffff");
    t.Set("--bg-secondary", "#f5f5fa");
    t.Set("--bg-tertiary", "#eeeef5");
    t.Set("--bg-surface", "#ffffff");
    t.Set("--text-primary", "#1a1a2e");
    t.Set("--text-secondary", "#555570");
    t.Set("--text-muted", "#8888aa");
    t.Set("--accent", "#3a3aee");
    t.Set("--accent-hover", "#4a4aff");
    t.Set("--accent-active", "#2a2add");
    t.Set("--success", "#22aa55");
    t.Set("--warning", "#dd8800");
    t.Set("--error", "#dd2244");
    t.Set("--border", "#00000015");
    t.Set("--border-focus", "#3a3aee80");
    t.Set("--shadow", "#00000020");
    return t;
  }
};

}  // namespace ugui

#endif  // ULTRAGUI_STYLE_THEME_H_
