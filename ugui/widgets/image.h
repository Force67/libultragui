#ifndef ULTRAGUI_WIDGETS_IMAGE_H_
#define ULTRAGUI_WIDGETS_IMAGE_H_

#include <ugui/rhi/rhi_types.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Texture shown by an image widget (WidgetKind::kImage). This is the widget's
/// data; its behaviour lives in ImageVTable(). An image is a generic Widget
/// carrying this component, not a subclass.
struct ImageContent {
  RHITextureHandle texture = kInvalidTexture;
  f32 natural_w = 0;
  f32 natural_h = 0;
};

/// Behaviour table (draw + measure) for image widgets.
WidgetVTable ImageVTable();

/// Create an image entity: a generic Widget tagged kImage with an empty
/// ImageContent component.
Widget* CreateImage(u32 id);

/// Set (or replace) the texture an image widget shows. No-op if `w` is null or
/// is not an image. Component-based replacement for the old Image::set_texture.
void SetImageTexture(Widget* w, RHITextureHandle texture, f32 width,
                     f32 height);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_IMAGE_H_
