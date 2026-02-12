#pragma once

#include <ultragui/core/rect.h>
#include <ultragui/core/types.h>
#include <ultragui/style/style.h>
#include <ultragui/widgets/widget_context.h>

#include <string>
#include <vector>

namespace ugui {

class Renderer2D;
struct LayoutNode;

/// Base class for all UI widgets. Provides lifecycle, tree structure,
/// hit-testing, and dirty-flag propagation.
class Widget {
public:
    explicit Widget(u32 id = 0) : id_(id) {}
    virtual ~Widget();

    // --- Tree management ---
    void add_child(Widget* child);
    void remove_child(Widget* child);
    Widget* child_at(u32 index) const;
    u32 child_count() const { return static_cast<u32>(children_.size()); }
    Widget* parent() const { return parent_; }
    const std::vector<Widget*>& children() const { return children_; }

    // --- Identity ---
    u32 id() const { return id_; }
    void set_id(u32 id) { id_ = id; }
    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }

    // --- Style ---
    Style& style() { return style_; }
    const Style& style() const { return style_; }
    void set_style(const Style& s) {
        style_ = s;
        mark_dirty();
    }

    void add_state_override(WidgetState state, const Style& override_style, u64 mask);
    WidgetState widget_state() const { return state_; }
    void set_widget_state(WidgetState state);

    /// Get the effective style (base + active overrides)
    Style computed_style() const;

    // --- Layout ---
    Rect rect() const { return rect_; }
    Rect content_rect() const { return content_rect_; }

    /// Set intrinsic content size (for text, images, etc.)
    void set_intrinsic_size(f32 w, f32 h) {
        intrinsic_w_ = w;
        intrinsic_h_ = h;
    }

    // --- Dirty flags ---
    void mark_dirty();
    void mark_paint_dirty() { paint_dirty_ = true; }
    bool is_dirty() const { return layout_dirty_; }
    bool is_paint_dirty() const { return paint_dirty_; }

    // --- Hit testing ---
    bool contains(Vec2 point) const { return rect_.contains(point); }
    Widget* hit_test(Vec2 point);

    // --- Context propagation ---
    void set_context(const WidgetContext* ctx);
    const WidgetContext* context() const { return context_; }

    // --- Lifecycle (override in subclasses) ---
    virtual void on_mount() {}
    virtual void on_unmount() {}
    virtual void on_update(f64 dt) {}
    virtual void on_layout(const Rect& rect, const Rect& content_rect);
    virtual void on_paint(Renderer2D& renderer);
    virtual void measure(f32& out_width, f32& out_height);

    // --- Event dispatch (override in subclasses) ---
    virtual bool on_click() { return false; }
    virtual bool on_scroll(Vec2 delta) { return false; }

    // --- Layout integration ---
    void populate_layout_node(LayoutNode& node) const;
    void apply_layout_result(const LayoutNode& node);

protected:
    u32 id_ = 0;
    std::string name_;
    Widget* parent_ = nullptr;
    std::vector<Widget*> children_;
    const WidgetContext* context_ = nullptr;

    Style style_;
    std::vector<StyleOverride> state_overrides_;
    WidgetState state_ = WidgetState::None;

    Rect rect_;
    Rect content_rect_;
    f32 intrinsic_w_ = 0, intrinsic_h_ = 0;
    bool layout_dirty_ = true;
    bool paint_dirty_ = true;
};

} // namespace ugui
