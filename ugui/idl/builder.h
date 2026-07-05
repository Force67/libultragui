#ifndef ULTRAGUI_IDL_BUILDER_H_
#define ULTRAGUI_IDL_BUILDER_H_

#include <ugui/idl/parser.h>
#include <ugui/style/style.h>
#include <ugui/text/text_engine.h>
#include <ugui/widgets/widget.h>

namespace ugui {

class Animator;

/// Component attached to widgets that declare `@media` blocks: the
/// media-independent base style plus the raw query property maps, so the
/// builder can re-resolve the overrides whenever the viewport changes
/// (responsive breakpoints keep working after window resizes, not just at
/// load time).
struct MediaStyle {
  struct Query {
    String condition;  // "min-width", "max-width", "min-height", "max-height"
    f32 value = 0.0f;
    HashMap<String, String> properties;
  };
  Style base;
  Vector<Query> queries;
};

/// Builds a widget tree from a parsed .ugui document.
/// Maps element types to widget constructors, resolves properties to styles.
class UguiBuilder {
 public:
  using WidgetFactory = Function<wid(const String& name)>;

  void RegisterType(const String& type_name, WidgetFactory factory);
  void set_animator(Animator* a) { animator_ = a; }
  void set_viewport_size(Vec2 size) { viewport_size_ = size; }

  /// Set a CSS custom property (e.g. "--accent", "#4a4aff").
  void SetVariable(const String& name, const String& value) {
    variables_[name] = value;
  }

  /// Register a named font so markup can select it per widget via `font: <name>`
  /// (e.g. a monospace face for code/IDs alongside the default UI font). The
  /// handle comes from UIContext::LoadFont. Unknown names in markup are ignored,
  /// leaving the widget on the context default font.
  void RegisterFont(const String& name, FontHandle handle) { fonts_[name] = handle; }

  /// Build a widget tree from a document. Returns the root entity.
  wid Build(const UguiDocument& doc);

  /// Rebuild: diff against previous tree and patch in-place.
  /// Returns the root entity (may be the same or new).
  wid Rebuild(const UguiDocument& doc, wid existing_root);

  /// Apply top-level `class <name> { ... }` style classes to an
  /// already-constructed widget. Used by application code to style
  /// dynamically-spawned widgets (chat bubbles, list rows, etc.)
  /// without rebuilding their style in C++. Accepts multiple
  /// space-separated names applied in order (later classes win).
  /// Returns true if at least one class was found.
  bool ApplyStyleClass(wid widget, const String& class_name) const;

  /// Look up a style class by name; returns nullptr if not found.
  const UguiDocument::StyleClass* FindStyleClass(const String& name) const;

  /// Re-evaluate `@media` overrides for every widget in the subtree against
  /// the current viewport size. UIContext calls this when the window
  /// resizes so breakpoints apply live.
  void ReapplyMediaQueries(wid root) const;

 private:
  wid BuildNode(const UguiNode& node, u32& id_counter);
  void ApplyProperties(wid widget, const UguiNode& node);
  void ApplyMediaStyle(wid widget) const;
  bool ApplyOneStyleClass(wid widget, const String& name) const;
  Style ParseStyle(const HashMap<String, String>& props) const;

  void CollectVariables(const UguiNode& node);
  String ResolveValue(const String& value) const;

  UguiNode ExpandComponent(const UguiDocument::Component& comp,
                           const UguiNode& instance) const;

  HashMap<String, WidgetFactory> factories_;
  HashMap<String, String> variables_;
  HashMap<String, FontHandle> fonts_;  // named fonts for the `font:` property
  HashMap<String, UguiDocument::StyleClass> style_classes_;
  HashMap<String, UguiDocument::Component> components_;
  Animator* animator_ = nullptr;
  Vec2 viewport_size_ = {1280.0f, 720.0f};
  u32 expand_depth_ = 0;  // recursion guard for nested components
};

}  // namespace ugui

#endif  // ULTRAGUI_IDL_BUILDER_H_
