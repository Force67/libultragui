#pragma once

/// Functional pipeline API for libultragui.
///
/// These free functions implement the core frame lifecycle as composable
/// stages. UIContext calls them in order as a convenience wrapper, but they
/// can be used directly for custom engine integration.
///
/// Typical frame sequence:
///
///   1. platform->poll_events()
///   2. input_router.process(root)           // route input to widgets
///   3. update_widget_tree(root, dt)          // tick scroll momentum etc.
///   4. text_engine.begin_frame()
///   5. measure_widget_tree(root)             // bottom-up text measurement
///   6. text_engine.flush_atlas()             // upload glyphs to GPU
///   7. rhi->begin_frame(clear_color)
///   8. renderer.begin_frame()
///   9. compute_widget_layout(root, vp, engine, scratch)
///  10. paint_widget_tree(root, renderer)     // depth-first rendering
///  11. text_engine.flush_atlas()             // catch late glyphs
///  12. renderer.end_frame()
///  13. rhi->end_frame()

#include <ultragui/layout/layout_tree.h>
#include <ultragui/render/paint.h>
#include <ultragui/widgets/widget_tree.h>
