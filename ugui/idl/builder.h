#ifndef ULTRAGUI_IDL_BUILDER_H_
#define ULTRAGUI_IDL_BUILDER_H_

#include <ugui/idl/parser.h>
#include <ugui/style/style.h>
#include <ugui/text/text_engine.h>
#include <ugui/widgets/widget.h>

namespace ugui {

class Animator;

/// Component for widgets declaring `@media` blocks: base style plus raw query
/// property maps, re-resolved on viewport changes so breakpoints stay live.
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

  /// Register a named font for the `font: <name>` property. Unknown names
  /// in markup are ignored.
  void RegisterFont(const String& name, FontHandle handle) { fonts_[name] = handle; }

  /// Build a widget tree from a document. Returns the root entity.
  wid Build(const UguiDocument& doc);

  /// Rebuild: diff against previous tree and patch in-place.
  /// Returns the root entity (may be the same or new).
  wid Rebuild(const UguiDocument& doc, wid existing_root);

  /// Apply top-level style classes to an already-built widget (space-separated
  /// names, later wins). For styling dynamically-spawned widgets without
  /// rebuilding style in C++. Returns true if any class matched.
  bool ApplyStyleClass(wid widget, const String& class_name) const;

  /// Look up a style class by name; returns nullptr if not found.
  const UguiDocument::StyleClass* FindStyleClass(const String& name) const;

  /// Re-evaluate `@media` overrides for a subtree against the current
  /// viewport. Called on window resize so breakpoints apply live.
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
