#ifndef ULTRAGUI_PIPELINE_H_
#define ULTRAGUI_PIPELINE_H_

/// Functional pipeline API for libultragui.
///
/// These free functions implement the core frame lifecycle as composable
/// stages. UIContext calls them in order as a convenience wrapper, but they
/// can be used directly for custom engine integration.
///
/// Typical frame sequence:
///
///   1. platform->PollEvents()
///   2. input_router.Process(root)           // route input to widgets
///   3. UpdateWidgetTree(root, dt)          // tick scroll momentum etc.
///   4. text_engine.BeginFrame()
///   5. MeasureWidgetTree(root)             // bottom-up text measurement
///   6. text_engine.FlushAtlas()             // upload glyphs to GPU
///   7. rhi->BeginFrame(clear_color)
///   8. renderer.BeginFrame()
///   9. ComputeWidgetLayout(root, vp, engine, scratch)
///  10. PaintWidgetTree(root, renderer)     // depth-first rendering
///  11. text_engine.FlushAtlas()             // catch late glyphs
///  12. renderer.EndFrame()
///  13. rhi->EndFrame()

#include <ultragui/layout/layout_tree.h>
#include <ultragui/render/paint.h>
#include <ultragui/widgets/widget_tree.h>

#endif  // ULTRAGUI_PIPELINE_H_
