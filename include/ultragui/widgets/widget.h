#ifndef ULTRAGUI_WIDGETS_WIDGET_H_
#define ULTRAGUI_WIDGETS_WIDGET_H_

#include <ultragui/core/rect.h>
#include <ultragui/core/types.h>
#include <ultragui/style/style.h>
#include <ultragui/style/transition.h>
#include <ultragui/widgets/widget_context.h>

namespace ugui {

class Renderer2D;
struct LayoutNode;

/// Base class for all UI widgets. Provides lifecycle, tree structure,
/// hit-testing, and dirty-flag propagation.
class Widget {
public:
    explicit Widget(u32 id = 0) : id_(id == 0 ? NextAutoId() : id) {}

    /// Generate a unique widget ID (thread-local counter).
    static u32 NextAutoId() {
        static u32 s_counter = 1;  // 0 is reserved / sentinel
        return s_counter++;
    }
    virtual ~Widget();

    // --- Tree management ---
    void AddChild(Widget* child);
    void RemoveChild(Widget* child);
    Widget* ChildAt(u32 index) const;
    u32 child_count() const { return static_cast<u32>(children_.size()); }
    Widget* parent() const { return parent_; }
    const Vector<Widget*>& children() const { return children_; }

    // --- Identity ---
    u32 id() const { return id_; }
    void set_id(u32 id) { id_ = id; }
    const String& name() const { return name_; }
    void set_name(const String& name) { name_ = name; }

    // --- Style ---
    Style& style() { return style_; }
    const Style& style() const { return style_; }
    void set_style(const Style& s) {
        style_ = s;
        MarkDirty();
    }

    void AddStateOverride(WidgetState state, const Style& override_style, u64 mask);
    void AddStateTransition(WidgetState state, const Transition& transition);
    WidgetState widget_state() const { return state_; }
    void set_widget_state(WidgetState state);

    /// Get the effective style (base + active overrides + animation)
    Style ComputedStyle() const;

    /// Set an animated style override (used by Animator callback)
    void SetAnimationStyle(const Style& s);
    void ClearAnimationStyle();

    // --- Layout ---
    Rect rect() const { return rect_; }
    Rect content_rect() const { return content_rect_; }

    /// Set intrinsic content size (for text, images, etc.)
    void set_intrinsic_size(f32 w, f32 h) {
        intrinsic_w_ = w;
        intrinsic_h_ = h;
    }

    // --- Dirty flags ---
    void MarkDirty();
    void MarkPaintDirty() { paint_dirty_ = true; }
    bool IsDirty() const { return layout_dirty_; }
    bool IsPaintDirty() const { return paint_dirty_; }

    // --- Hit testing ---
    bool contains(Vec2 point) const { return rect_.contains(point); }
    virtual Widget* HitTest(Vec2 point);

    /// Convert a screen/input point into this widget's layout space by
    /// accounting for ancestor scroll offsets.
    Vec2 InputToLayoutPoint(Vec2 point) const;

    // --- Context propagation ---
    void SetContext(const WidgetContext* ctx);
    const WidgetContext* context() const { return context_; }

    // --- Lifecycle (override in subclasses) ---
    virtual void OnMount() {}
    virtual void OnUnmount() {}
    virtual void OnUpdate(f64 dt) {}
    virtual void OnLayout(const Rect& rect, const Rect& content_rect);
    virtual void OnPaint(Renderer2D& renderer);
    virtual void Measure(f32& out_width, f32& out_height);

    // --- Tooltip ---
    void set_tooltip(const String& text) { tooltip_ = text; }
    const String& tooltip() const { return tooltip_; }

    // --- Tab navigation ---
    void set_tab_index(i32 idx) { tab_index_ = idx; }
    i32 tab_index() const { return tab_index_; }
    bool focusable() const { return tab_index_ >= 0; }

    /// Viewport scale factor (1.0 at the design resolution).
    f32 ui_scale() const { return context_ ? context_->ui_scale : 1.0f; }

    // --- Event dispatch (override in subclasses) ---
    virtual bool OnClick() { return false; }
    virtual bool OnScroll(Vec2 delta) { return false; }
    virtual bool OnKeyDown(i32 key, i32 mods) { return false; }
    virtual bool OnKeyUp(i32 key, i32 mods) { return false; }
    virtual bool OnCharInput(u32 codepoint) { return false; }
    virtual void OnDragStart(Vec2 pos) {}
    virtual void OnDragMove(Vec2 pos, Vec2 delta) {}
    virtual void OnDragEnd(Vec2 pos) {}

    // --- Layout integration ---
    void PopulateLayoutNode(LayoutNode& node) const;
    void ApplyLayoutResult(const LayoutNode& node);

protected:
    u32 id_ = 0;
    String name_;
    String tooltip_;
    Widget* parent_ = nullptr;
    Vector<Widget*> children_;
    const WidgetContext* context_ = nullptr;

    Style style_;
    Vector<StyleOverride> state_overrides_;
    WidgetState state_ = WidgetState::kNone;

    struct StateTransitionConfig {
        WidgetState state;
        Transition transition;
    };
    Vector<StateTransitionConfig> state_transitions_;

    Optional<Style> animation_style_;

    Rect rect_;
    Rect content_rect_;
    f32 intrinsic_w_ = 0, intrinsic_h_ = 0;
    i32 tab_index_ = -1;  // -1 = not focusable via tab, 0+ = tab order
    bool layout_dirty_ = true;
    bool paint_dirty_ = true;
};

} // namespace ugui

#endif  // ULTRAGUI_WIDGETS_WIDGET_H_
