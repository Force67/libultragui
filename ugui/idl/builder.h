#ifndef ULTRAGUI_IDL_BUILDER_H_
#define ULTRAGUI_IDL_BUILDER_H_

#include <ugui/idl/parser.h>
#include <ugui/style/style.h>
#include <ugui/text/text_engine.h>
#include <ugui/widgets/widget.h>

namespace ugui {

class Animator;

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

  /// Apply a top-level `class <name> { ... }` style class to an
  /// already-constructed widget. Used by application code to style
  /// dynamically-spawned widgets (chat bubbles, list rows, etc.)
  /// without rebuilding their style in C++. Returns true if the
  /// class was found.
  bool ApplyStyleClass(wid widget, const String& class_name) const;

  /// Look up a style class by name; returns nullptr if not found.
  const UguiDocument::StyleClass* FindStyleClass(const String& name) const;

 private:
  wid BuildNode(const UguiNode& node, u32& id_counter);
  void ApplyProperties(wid widget, const UguiNode& node);
  Style ParseStyle(const HashMap<String, String>& props) const;

  void CollectVariables(const UguiNode& node);
  String ResolveValue(const String& value) const;

  HashMap<String, WidgetFactory> factories_;
  HashMap<String, String> variables_;
  HashMap<String, FontHandle> fonts_;  // named fonts for the `font:` property
  HashMap<String, UguiDocument::StyleClass> style_classes_;
  Animator* animator_ = nullptr;
  Vec2 viewport_size_ = {1280.0f, 720.0f};
};

}  // namespace ugui

#endif  // ULTRAGUI_IDL_BUILDER_H_
