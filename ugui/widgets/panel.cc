#include <ugui/widgets/panel.h>
#include <ugui/widgets/widget.h>
#include <ugui/widgets/widget_registry.h>

namespace ugui {

wid CreatePanel(u32 id) {
  WidgetRegistry& world = *WidgetRegistry::Active();
  wid e = world.New(id);
  world.Get<WidgetNode>(e)->kind = WidgetKind::kPanel;
  return e;
}

}  // namespace ugui
