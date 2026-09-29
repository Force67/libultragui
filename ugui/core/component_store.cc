#include <ugui/core/component_store.h>

namespace ugui {

#if defined(_WIN32)
u32 ComponentTypeIdFor(const char* type_signature) {
  static HashMap<String, u32> ids;
  if (const u32* id = ids.find(String(type_signature))) return *id;
  const u32 id = static_cast<u32>(ids.size());
  ids[String(type_signature)] = id;
  return id;
}
#endif

}  // namespace ugui
