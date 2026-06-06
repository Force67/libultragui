#ifndef ULTRAGUI_WIDGETS_IMAGE_H_
#define ULTRAGUI_WIDGETS_IMAGE_H_

#include <ugui/rhi/rhi_types.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_vtable.h>

namespace ugui {

/// Texture shown by an image widget (WidgetKind::kImage). This is the widget's
/// data; its behaviour lives in ImageVTable(). An image is a generic widget
/// entity carrying this component, not a subclass.
struct ImageContent {
  RHITextureHandle texture = kInvalidTexture;
  f32 natural_w = 0;
  f32 natural_h = 0;
};

/// Behaviour table (draw + measure) for image widgets.
UGUI_API WidgetVTable ImageVTable();

/// Create an image entity: a generic widget tagged kImage with an empty
/// ImageContent component.
UGUI_API wid CreateImage(u32 id);

/// Set (or replace) the texture an image widget shows. No-op if `e` is not an
/// image. Component-based replacement for the old Image::set_texture.
UGUI_API void SetImageTexture(wid e, RHITextureHandle texture, f32 width,
                              f32 height);

}  // namespace ugui

#endif  // ULTRAGUI_WIDGETS_IMAGE_H_
