#define GLFW_INCLUDE_NONE
#include <ultragui/input/input.h>
#include <ultragui/style/style.h>
#include <ultragui/widgets/button.h>
#include <ultragui/widgets/scroll_view.h>
#include <ultragui/widgets/widget.h>

#include <GLFW/glfw3.h>

namespace ugui {

void InputRouter::init(Platform* platform) {
    platform_ = platform;
    install_callbacks();
}

void InputRouter::install_callbacks() {
    auto* window = static_cast<GLFWwindow*>(platform_->native_handle());

    // Store 'this' for callback access
    glfwSetWindowUserPointer(window, this);

    glfwSetCursorPosCallback(window, glfw_mouse_pos_callback);
    glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
    glfwSetScrollCallback(window, glfw_scroll_callback);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetCharCallback(window, glfw_char_callback);
}

// ---------------------------------------------------------------------------
// GLFW callbacks - push events into queues
// ---------------------------------------------------------------------------

void InputRouter::glfw_mouse_pos_callback(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<InputRouter*>(glfwGetWindowUserPointer(window));
    if (self->move_count_ < MAX_EVENTS) {
        self->move_events_[self->move_count_++] = {{static_cast<f32>(x), static_cast<f32>(y)}};
    }
    self->mouse_pos_ = {static_cast<f32>(x), static_cast<f32>(y)};
}

void InputRouter::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int) {
    auto* self = static_cast<InputRouter*>(glfwGetWindowUserPointer(window));
    if (self->button_count_ < MAX_EVENTS) {
        self->button_events_[self->button_count_++] = {static_cast<MouseButton>(button),
                                                       action == GLFW_PRESS, self->mouse_pos_};
    }
}

void InputRouter::glfw_scroll_callback(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<InputRouter*>(glfwGetWindowUserPointer(window));
    if (self->scroll_count_ < MAX_EVENTS) {
        self->scroll_events_[self->scroll_count_++] = {{static_cast<f32>(x), static_cast<f32>(y)},
                                                       self->mouse_pos_};
    }
}

void InputRouter::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action,
                                    int mods) {
    auto* self = static_cast<InputRouter*>(glfwGetWindowUserPointer(window));
    if (self->key_count_ < MAX_EVENTS) {
        self->key_events_[self->key_count_++] = {key, scancode, action == GLFW_PRESS,
                                                 action == GLFW_REPEAT, mods};
    }
}

void InputRouter::glfw_char_callback(GLFWwindow*, unsigned int) {
    // For future TextInput widget
}

// ---------------------------------------------------------------------------
// Process input each frame
// ---------------------------------------------------------------------------

bool InputRouter::process(Widget* root) {
    if (!root)
        return false;

    bool consumed = false;

    // Process mouse movement -> hover detection
    if (move_count_ > 0) {
        Vec2 pos = move_events_[move_count_ - 1].position; // Use latest position
        Widget* new_hover = root->hit_test(pos);

        if (new_hover != hovered_) {
            if (hovered_) {
                auto state = hovered_->widget_state();
                hovered_->set_widget_state(static_cast<WidgetState>(
                    static_cast<u16>(state) & ~static_cast<u16>(WidgetState::Hovered)));
                if (on_hover_)
                    on_hover_(hovered_, false);
            }
            hovered_ = new_hover;
            if (hovered_) {
                hovered_->set_widget_state(hovered_->widget_state() | WidgetState::Hovered);
                if (on_hover_)
                    on_hover_(hovered_, true);

                // Update cursor based on hovered widget's style
                Cursor cur = hovered_->computed_style().cursor;
                platform_->set_cursor(cur);
            } else {
                platform_->set_cursor(Cursor::Auto);
            }
            consumed = true;
        }
        move_count_ = 0;
    }

    // Process mouse buttons -> press/click
    for (u32 i = 0; i < button_count_; ++i) {
        auto& evt = button_events_[i];
        Widget* target = root->hit_test(evt.position);

        if (evt.pressed) {
            pressed_ = target;
            if (target) {
                target->set_widget_state(target->widget_state() | WidgetState::Pressed);
            }
            // Update focus
            set_focus(target);
            consumed = true;
        } else {
            // Release
            if (pressed_) {
                auto state = pressed_->widget_state();
                pressed_->set_widget_state(static_cast<WidgetState>(
                    static_cast<u16>(state) & ~static_cast<u16>(WidgetState::Pressed)));

                // Click: press and release on same widget
                if (pressed_ == target && target) {
                    if (on_click_)
                        on_click_(target, evt.button);

                    // Trigger button click handler
                    if (auto* btn = dynamic_cast<Button*>(target)) {
                        btn->click();
                    }
                }
            }
            pressed_ = nullptr;
            consumed = true;
        }
    }
    button_count_ = 0;

    // Process scroll -> scroll view
    for (u32 i = 0; i < scroll_count_; ++i) {
        auto& evt = scroll_events_[i];
        Widget* target = root->hit_test(evt.position);

        // Walk up the tree to find a ScrollView
        Widget* w = target;
        while (w) {
            if (auto* sv = dynamic_cast<ScrollView*>(w)) {
                sv->scroll_by(Vec2{-evt.delta.x * 40.0f, -evt.delta.y * 40.0f});
                consumed = true;
                break;
            }
            w = w->parent();
        }
    }
    scroll_count_ = 0;

    key_count_ = 0;

    return consumed;
}

void InputRouter::set_focus(Widget* widget) {
    if (focused_ == widget)
        return;

    if (focused_) {
        auto state = focused_->widget_state();
        focused_->set_widget_state(static_cast<WidgetState>(
            static_cast<u16>(state) & ~static_cast<u16>(WidgetState::Focused)));
    }
    focused_ = widget;
    if (focused_) {
        focused_->set_widget_state(focused_->widget_state() | WidgetState::Focused);
    }
}

} // namespace ugui
