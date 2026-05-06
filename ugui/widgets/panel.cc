#include <ugui/widgets/panel.h>

namespace ugui {

Widget* CreatePanel(u32 id) {
  Widget* w = new Widget(id);
  w->set_kind(WidgetKind::kPanel);
  return w;
}

}  // namespace ugui
