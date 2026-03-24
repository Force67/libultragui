#include <ugui/platform/platform.h>
#include <ugui/scripting/script_runtime.h>
#include <ugui/widgets/button.h>
#include <ugui/widgets/checkbox.h>
#include <ugui/widgets/dropdown.h>
#include <ugui/widgets/slider.h>
#include <ugui/widgets/text.h>
#include <ugui/widgets/widget.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>

namespace ugui {

// Registry key for storing the Impl* in Lua's registry
static const char* const REGISTRY_KEY = "ugui_runtime";

struct ScriptRuntime::Impl {
  lua_State* L = nullptr;
  HashMap<String, Widget*> widget_registry;
  Vector<ScriptRuntime::NativeFunction*> native_functions;

  struct TimerEntry {
    double fire_time;
    int func_ref;  // LUA_REGISTRYINDEX reference
  };
  Vector<TimerEntry> timers;
  double current_time = 0.0;

  struct TweenEntry {
    String widget_name;
    String property;
    double start_time;
    double duration;
    f32 from_num, to_num;
    f32 from_r, from_g, from_b, from_a;
    f32 to_r, to_g, to_b, to_a;
    bool is_color;
    u8 easing;  // 0=linear, 1=ease-in, 2=ease-out, 3=ease-in-out
  };
  Vector<TweenEntry> tweens;

  static f64 ApplyEasing(f64 t, u8 easing) {
    switch (easing) {
      case 1:
        return t * t;  // ease-in
      case 2:
        return 1.0 - (1.0 - t) * (1.0 - t);  // ease-out
      case 3:
        return t < 0.5 ? 2 * t * t
                       : 1 - ((-2 * t + 2) * (-2 * t + 2)) / 2;  // ease-in-out
      default:
        return t;  // linear
    }
  }

  static u8 ParseEasing(const char* s) {
    if (!s) return 0;
    if (strcmp(s, "ease-in") == 0) return 1;
    if (strcmp(s, "ease-out") == 0) return 2;
    if (strcmp(s, "ease-in-out") == 0) return 3;
    return 0;
  }

  static bool ParseHexColor(const char* val, f32& r, f32& g, f32& b, f32& a) {
    if (!val || val[0] != '#') return false;
    size_t len = strlen(val);
    if (len != 7 && len != 9) return false;
    u32 hex = 0;
    std::from_chars(val + 1, val + 7, hex, 16);
    r = static_cast<f32>((hex >> 16) & 0xFF);
    g = static_cast<f32>((hex >> 8) & 0xFF);
    b = static_cast<f32>(hex & 0xFF);
    a = 255.0f;
    if (len == 9) {
      u32 alpha = 0;
      std::from_chars(val + 7, val + 9, alpha, 16);
      a = static_cast<f32>(alpha);
    }
    return true;
  }

  static Impl* FromState(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
    auto* rt = static_cast<Impl*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return rt;
  }

  void RegisterApi();
  static int LuaUguiFind(lua_State* L);
  static int LuaUguiGetProp(lua_State* L);
  static int LuaUguiSetProp(lua_State* L);
  static int LuaUguiLog(lua_State* L);
  static int LuaUguiAfter(lua_State* L);
  static int LuaUguiTween(lua_State* L);
  static int LuaUguiOpenUrl(lua_State* L);

  // Push widget table with type-specific fields onto Lua stack.
  void PushWidgetTable(Widget* widget);
};

ScriptRuntime::ScriptRuntime() : impl_(new Impl()) {}
ScriptRuntime::~ScriptRuntime() { delete impl_; }

bool ScriptRuntime::Init() {
  impl_->L = luaL_newstate();
  if (!impl_->L) return false;

  // Open safe standard libraries (no io/os for sandboxing)
  luaL_requiref(impl_->L, "_G", luaopen_base, 1);
  lua_pop(impl_->L, 1);
  luaL_requiref(impl_->L, LUA_TABLIBNAME, luaopen_table, 1);
  lua_pop(impl_->L, 1);
  luaL_requiref(impl_->L, LUA_STRLIBNAME, luaopen_string, 1);
  lua_pop(impl_->L, 1);
  luaL_requiref(impl_->L, LUA_MATHLIBNAME, luaopen_math, 1);
  lua_pop(impl_->L, 1);

  // Store impl in Lua's registry so static callbacks can find us
  lua_pushlightuserdata(impl_->L, impl_);
  lua_setfield(impl_->L, LUA_REGISTRYINDEX, REGISTRY_KEY);

  impl_->RegisterApi();
  return true;
}

void ScriptRuntime::Shutdown() {
  if (impl_->L) {
    lua_close(impl_->L);
    impl_->L = nullptr;
  }
  for (auto* fn : impl_->native_functions) {
    delete fn;
  }
  impl_->native_functions.clear();
  impl_->widget_registry.clear();
}

bool ScriptRuntime::Exec(const char* script, const char* name) {
  if (luaL_loadbuffer(impl_->L, script, strlen(script), name) != LUA_OK) {
    std::fprintf(stderr, "ugui/lua: load error: %s\n",
                 lua_tostring(impl_->L, -1));
    lua_pop(impl_->L, 1);
    return false;
  }
  if (lua_pcall(impl_->L, 0, 0, 0) != LUA_OK) {
    std::fprintf(stderr, "ugui/lua: runtime error: %s\n",
                 lua_tostring(impl_->L, -1));
    lua_pop(impl_->L, 1);
    return false;
  }
  return true;
}

bool ScriptRuntime::ExecFile(const char* path) {
  if (luaL_loadfile(impl_->L, path) != LUA_OK) {
    std::fprintf(stderr, "ugui/lua: failed to load '%s': %s\n", path,
                 lua_tostring(impl_->L, -1));
    lua_pop(impl_->L, 1);
    return false;
  }
  if (lua_pcall(impl_->L, 0, 0, 0) != LUA_OK) {
    std::fprintf(stderr, "ugui/lua: error in '%s': %s\n", path,
                 lua_tostring(impl_->L, -1));
    lua_pop(impl_->L, 1);
    return false;
  }
  return true;
}

void ScriptRuntime::RegisterWidget(Widget* widget) {
  if (!widget->name().empty()) {
    impl_->widget_registry[widget->name()] = widget;
  }
}

void ScriptRuntime::UnregisterWidget(Widget* widget) {
  if (!widget->name().empty()) {
    impl_->widget_registry.erase(widget->name());
  }
}

void ScriptRuntime::ClearWidgetRegistry() { impl_->widget_registry.clear(); }

Widget* ScriptRuntime::FindRegisteredWidget(const char* name) const {
  auto it = impl_->widget_registry.find(name);
  return it != impl_->widget_registry.end() ? it->second : nullptr;
}

void ScriptRuntime::Impl::PushWidgetTable(Widget* widget) {
  lua_newtable(L);
  lua_pushstring(L, widget->name().c_str());
  lua_setfield(L, -2, "name");
  lua_pushinteger(L, widget->id());
  lua_setfield(L, -2, "id");

  // Type-specific fields so Lua handlers get w.checked, w.selected, w.value
  if (auto* cb = dynamic_cast<Checkbox*>(widget)) {
    lua_pushboolean(L, cb->checked());
    lua_setfield(L, -2, "checked");
  }
  if (auto* dd = dynamic_cast<Dropdown*>(widget)) {
    lua_pushinteger(L, dd->selected_index());
    lua_setfield(L, -2, "selected");
    lua_pushstring(L, dd->selected_text().c_str());
    lua_setfield(L, -2, "selected_text");
  }
  if (auto* sl = dynamic_cast<Slider*>(widget)) {
    lua_pushnumber(L, sl->value());
    lua_setfield(L, -2, "value");
    lua_pushnumber(L, sl->min());
    lua_setfield(L, -2, "min");
    lua_pushnumber(L, sl->max());
    lua_setfield(L, -2, "max");
  }
}

bool ScriptRuntime::CallHandler(const char* func_name, Widget* widget) {
  lua_getglobal(impl_->L, func_name);
  if (!lua_isfunction(impl_->L, -1)) {
    lua_pop(impl_->L, 1);
    return false;
  }

  impl_->PushWidgetTable(widget);

  if (lua_pcall(impl_->L, 1, 0, 0) != LUA_OK) {
    std::fprintf(stderr, "ugui/lua: error calling '%s': %s\n", func_name,
                 lua_tostring(impl_->L, -1));
    lua_pop(impl_->L, 1);
    return false;
  }
  return true;
}

void ScriptRuntime::ScheduleTimer(double delay_seconds, int lua_func_ref) {
  impl_->timers.push_back({impl_->current_time + delay_seconds, lua_func_ref});
}

void ScriptRuntime::SyncTimerClock(double current_time) {
  impl_->current_time = current_time;
}

void ScriptRuntime::ClearTimersAndTweens() {
  // Release Lua references for pending timer callbacks.
  for (auto& entry : impl_->timers) {
    luaL_unref(impl_->L, LUA_REGISTRYINDEX, entry.func_ref);
  }
  impl_->timers.clear();
  impl_->tweens.clear();
}

void ScriptRuntime::UpdateTimers(double current_time) {
  impl_->current_time = current_time;
  // Process in a separate pass to avoid iterator invalidation if callbacks
  // schedule more timers.
  Vector<Impl::TimerEntry> ready;
  auto& timers = impl_->timers;
  for (size_t i = 0; i < timers.size();) {
    if (current_time >= timers[i].fire_time) {
      ready.push_back(timers[i]);
      timers[i] = timers.back();
      timers.pop_back();
    } else {
      ++i;
    }
  }
  for (auto& entry : ready) {
    lua_rawgeti(impl_->L, LUA_REGISTRYINDEX, entry.func_ref);
    if (lua_pcall(impl_->L, 0, 0, 0) != LUA_OK) {
      std::fprintf(stderr, "ugui/lua: timer error: %s\n",
                   lua_tostring(impl_->L, -1));
      lua_pop(impl_->L, 1);
    }
    luaL_unref(impl_->L, LUA_REGISTRYINDEX, entry.func_ref);
  }

  // Process active tweens
  auto* L = impl_->L;
  for (size_t i = 0; i < impl_->tweens.size();) {
    auto& tw = impl_->tweens[i];
    f64 elapsed = current_time - tw.start_time;
    f64 t = elapsed / tw.duration;
    if (t > 1.0) t = 1.0;
    if (t < 0.0) t = 0.0;
    t = Impl::ApplyEasing(t, tw.easing);

    // Call ugui.set(widget, property, value) via the Lua API
    lua_getglobal(L, "ugui");
    lua_getfield(L, -1, "set");
    lua_pushstring(L, tw.widget_name.c_str());
    lua_pushstring(L, tw.property.c_str());

    if (tw.is_color) {
      auto lerp = [](f32 a, f32 b, f64 t) -> int {
        return std::clamp(static_cast<int>(a + (b - a) * t), 0, 255);
      };
      int r = lerp(tw.from_r, tw.to_r, t);
      int g = lerp(tw.from_g, tw.to_g, t);
      int b = lerp(tw.from_b, tw.to_b, t);
      int a = lerp(tw.from_a, tw.to_a, t);
      char hex[10];
      std::snprintf(hex, sizeof(hex), "#%02x%02x%02x%02x", r, g, b, a);
      lua_pushstring(L, hex);
    } else {
      f32 v = tw.from_num + (tw.to_num - tw.from_num) * static_cast<f32>(t);
      lua_pushnumber(L, v);
    }

    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
      lua_pop(L, 1);
    }
    lua_pop(L, 1);  // pop ugui table

    if (elapsed >= tw.duration) {
      impl_->tweens[i] = impl_->tweens.back();
      impl_->tweens.pop_back();
    } else {
      ++i;
    }
  }
}

static void WireChangeHandlersRecursive(ScriptRuntime& rt, Widget* w) {
  if (!w) return;
  const auto& name = w->name();
  if (!name.empty()) {
    if (auto* dd = dynamic_cast<Dropdown*>(w)) {
      dd->set_on_change([&rt, widget = w](i32, const String&) {
        std::string handler = "on_" + widget->name();
        rt.CallHandler(handler.c_str(), widget);
      });
    }
    if (auto* cb = dynamic_cast<Checkbox*>(w)) {
      cb->set_on_change([&rt, widget = w](bool) {
        std::string handler = "on_" + widget->name();
        rt.CallHandler(handler.c_str(), widget);
      });
    }
    if (auto* sl = dynamic_cast<Slider*>(w)) {
      sl->set_on_change([&rt, widget = w](f32) {
        std::string handler = "on_" + widget->name();
        rt.CallHandler(handler.c_str(), widget);
      });
    }
  }
  for (u32 i = 0; i < w->child_count(); ++i)
    WireChangeHandlersRecursive(rt, w->ChildAt(i));
}

void ScriptRuntime::WireChangeHandlers(Widget* root) {
  WireChangeHandlersRecursive(*this, root);
}

void ScriptRuntime::RegisterFunction(const char* name, NativeFunction func) {
  lua_getglobal(impl_->L, "ugui");
  if (!lua_istable(impl_->L, -1)) {
    lua_pop(impl_->L, 1);
    return;
  }

  auto* fn = new NativeFunction(std::move(func));
  impl_->native_functions.push_back(fn);
  lua_pushlightuserdata(impl_->L, fn);
  lua_pushcclosure(
      impl_->L,
      [](lua_State* ls) -> int {
        auto* f = static_cast<NativeFunction*>(
            lua_touserdata(ls, lua_upvalueindex(1)));
        return (*f)(ls);
      },
      1);
  lua_setfield(impl_->L, -2, name);
  lua_pop(impl_->L, 1);
}

lua_State* ScriptRuntime::state() const { return impl_->L; }

// ---------------------------------------------------------------------------
// Lua API bindings
// ---------------------------------------------------------------------------

void ScriptRuntime::Impl::RegisterApi() {
  lua_newtable(L);

  lua_pushcfunction(L, LuaUguiFind);
  lua_setfield(L, -2, "find");

  lua_pushcfunction(L, LuaUguiGetProp);
  lua_setfield(L, -2, "get");

  lua_pushcfunction(L, LuaUguiSetProp);
  lua_setfield(L, -2, "set");

  lua_pushcfunction(L, LuaUguiLog);
  lua_setfield(L, -2, "log");

  lua_pushcfunction(L, LuaUguiAfter);
  lua_setfield(L, -2, "after");

  lua_pushcfunction(L, LuaUguiTween);
  lua_setfield(L, -2, "tween");

  lua_pushcfunction(L, LuaUguiOpenUrl);
  lua_setfield(L, -2, "open_url");

  lua_setglobal(L, "ugui");
}

int ScriptRuntime::Impl::LuaUguiAfter(lua_State* L) {
  auto* impl = FromState(L);
  double delay = luaL_checknumber(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_pushvalue(L, 2);
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  impl->timers.push_back({impl->current_time + delay, ref});
  return 0;
}

int ScriptRuntime::Impl::LuaUguiTween(lua_State* L) {
  auto* impl = FromState(L);
  const char* name = luaL_checkstring(L, 1);
  const char* prop = luaL_checkstring(L, 2);

  TweenEntry tw;
  tw.widget_name = name;
  tw.property = prop;
  tw.start_time = impl->current_time;
  tw.easing = 0;
  tw.is_color = false;
  tw.from_num = tw.to_num = 0;
  tw.from_r = tw.from_g = tw.from_b = tw.from_a = 0;
  tw.to_r = tw.to_g = tw.to_b = tw.to_a = 0;

  if (lua_type(L, 3) == LUA_TSTRING) {
    tw.is_color = true;
    ParseHexColor(lua_tostring(L, 3), tw.from_r, tw.from_g, tw.from_b,
                  tw.from_a);
    ParseHexColor(luaL_checkstring(L, 4), tw.to_r, tw.to_g, tw.to_b, tw.to_a);
    tw.duration = luaL_checknumber(L, 5);
    if (lua_gettop(L) >= 6 && lua_type(L, 6) == LUA_TSTRING)
      tw.easing = ParseEasing(lua_tostring(L, 6));
  } else {
    tw.from_num = static_cast<f32>(luaL_checknumber(L, 3));
    tw.to_num = static_cast<f32>(luaL_checknumber(L, 4));
    tw.duration = luaL_checknumber(L, 5);
    if (lua_gettop(L) >= 6 && lua_type(L, 6) == LUA_TSTRING)
      tw.easing = ParseEasing(lua_tostring(L, 6));
  }

  // Cancel existing tween on same widget+property
  auto& tweens = impl->tweens;
  for (size_t i = 0; i < tweens.size();) {
    if (tweens[i].widget_name == tw.widget_name &&
        tweens[i].property == tw.property) {
      tweens[i] = tweens.back();
      tweens.pop_back();
    } else {
      ++i;
    }
  }

  tweens.push_back(std::move(tw));
  return 0;
}

int ScriptRuntime::Impl::LuaUguiOpenUrl(lua_State* L) {
  const char* url = luaL_checkstring(L, 1);
  Platform::OpenURL(url);
  return 0;
}

int ScriptRuntime::Impl::LuaUguiFind(lua_State* L) {
  auto* rt = FromState(L);
  const char* name = luaL_checkstring(L, 1);

  auto it = rt->widget_registry.find(name);
  Widget* w = (it != rt->widget_registry.end()) ? it->second : nullptr;
  if (!w) {
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);
  lua_pushstring(L, w->name().c_str());
  lua_setfield(L, -2, "name");
  lua_pushinteger(L, w->id());
  lua_setfield(L, -2, "id");

  return 1;
}

int ScriptRuntime::Impl::LuaUguiGetProp(lua_State* L) {
  auto* rt = FromState(L);
  const char* name = luaL_checkstring(L, 1);
  const char* prop = luaL_checkstring(L, 2);

  auto it = rt->widget_registry.find(name);
  Widget* w = (it != rt->widget_registry.end()) ? it->second : nullptr;
  if (!w) {
    lua_pushnil(L);
    return 1;
  }

  const Style& s = w->style();

  if (strcmp(prop, "opacity") == 0)
    lua_pushnumber(L, s.opacity);
  else if (strcmp(prop, "visible") == 0)
    lua_pushboolean(L, s.visibility == Visibility::kVisible);
  else if (strcmp(prop, "font-size") == 0)
    lua_pushnumber(L, s.font_size);
  else if (strcmp(prop, "corner-radius") == 0)
    lua_pushnumber(L, s.corner_radius);
  else
    lua_pushnil(L);

  return 1;
}

int ScriptRuntime::Impl::LuaUguiSetProp(lua_State* L) {
  auto* rt = FromState(L);
  const char* name = luaL_checkstring(L, 1);
  const char* prop = luaL_checkstring(L, 2);

  auto it = rt->widget_registry.find(name);
  Widget* w = (it != rt->widget_registry.end()) ? it->second : nullptr;
  if (!w) {
    std::fprintf(stderr, "ugui/lua: ugui.set: widget '%s' not found\n",
                 name);
    return 0;
  }

  Style s = w->style();

  if (strcmp(prop, "opacity") == 0) {
    s.opacity = static_cast<f32>(luaL_checknumber(L, 3));
  } else if (strcmp(prop, "visible") == 0) {
    s.visibility =
        lua_toboolean(L, 3) ? Visibility::kVisible : Visibility::kHidden;
    w->set_style(s);
    w->ClearAnimationStyle();
    return 0;
  } else if (strcmp(prop, "font-size") == 0) {
    s.font_size = static_cast<f32>(luaL_checknumber(L, 3));
  } else if (strcmp(prop, "corner-radius") == 0) {
    f32 r = static_cast<f32>(luaL_checknumber(L, 3));
    s.corner_radius = r;
    s.corner_radius_tl = r;
    s.corner_radius_tr = r;
    s.corner_radius_br = r;
    s.corner_radius_bl = r;
  } else if (strcmp(prop, "color") == 0) {
    const char* val = luaL_checkstring(L, 3);
    size_t vlen = strlen(val);
    if (val[0] == '#' && (vlen == 7 || vlen == 9)) {
      u32 hex = 0;
      std::from_chars(val + 1, val + 7, hex, 16);
      s.text_color = Color::FromHex(hex);
      if (vlen == 9) {
        u32 alpha = 0;
        std::from_chars(val + 7, val + 9, alpha, 16);
        s.text_color = s.text_color.WithAlpha(static_cast<f32>(alpha) / 255.0f);
      }
    }
  } else if (strcmp(prop, "text-shadow-color") == 0) {
    const char* val = luaL_checkstring(L, 3);
    size_t vlen = strlen(val);
    if (val[0] == '#' && (vlen == 7 || vlen == 9)) {
      u32 hex = 0;
      std::from_chars(val + 1, val + 7, hex, 16);
      s.text_shadow_color = Color::FromHex(hex);
      if (vlen == 9) {
        u32 alpha = 0;
        std::from_chars(val + 7, val + 9, alpha, 16);
        s.text_shadow_color =
            s.text_shadow_color.WithAlpha(static_cast<f32>(alpha) / 255.0f);
      }
    }
  } else if (strcmp(prop, "shadow-color") == 0) {
    const char* val = luaL_checkstring(L, 3);
    size_t vlen = strlen(val);
    if (val[0] == '#' && (vlen == 7 || vlen == 9)) {
      u32 hex = 0;
      std::from_chars(val + 1, val + 7, hex, 16);
      s.shadow.color = Color::FromHex(hex);
      if (vlen == 9) {
        u32 alpha = 0;
        std::from_chars(val + 7, val + 9, alpha, 16);
        s.shadow.color =
            s.shadow.color.WithAlpha(static_cast<f32>(alpha) / 255.0f);
      }
    }
  } else if (strcmp(prop, "shadow-blur") == 0) {
    s.shadow.blur = static_cast<f32>(luaL_checknumber(L, 3));
  } else if (strcmp(prop, "cursor") == 0) {
    const char* val = luaL_checkstring(L, 3);
    if (strcmp(val, "pointer") == 0)
      s.cursor = Cursor::kPointer;
    else if (strcmp(val, "text") == 0)
      s.cursor = Cursor::kText;
    else if (strcmp(val, "move") == 0)
      s.cursor = Cursor::kMove;
    else if (strcmp(val, "not-allowed") == 0)
      s.cursor = Cursor::kNotAllowed;
    else
      s.cursor = Cursor::kDefault;
  } else if (strcmp(prop, "background") == 0) {
    const char* val = luaL_checkstring(L, 3);
    if (strcmp(val, "transparent") == 0) {
      s.background = Color::Transparent();
    } else {
      size_t vlen = strlen(val);
      if (val[0] == '#' && (vlen == 7 || vlen == 9)) {
        u32 hex = 0;
        std::from_chars(val + 1, val + 7, hex, 16);
        s.background = Color::FromHex(hex);
        if (vlen == 9) {
          u32 alpha = 0;
          std::from_chars(val + 7, val + 9, alpha, 16);
          s.background =
              s.background.WithAlpha(static_cast<f32>(alpha) / 255.0f);
        }
      }
    }
  } else if (strcmp(prop, "width") == 0) {
    if (lua_type(L, 3) == LUA_TSTRING) {
      const char* val = lua_tostring(L, 3);
      size_t len = strlen(val);
      if (len > 1 && val[len - 1] == '%') {
        s.width = Length::Percent(static_cast<f32>(atof(val)));
      } else {
        s.width = Length::Px(static_cast<f32>(atof(val)));
      }
    } else {
      s.width = Length::Px(static_cast<f32>(luaL_checknumber(L, 3)));
    }
  } else if (strcmp(prop, "height") == 0) {
    if (lua_type(L, 3) == LUA_TSTRING) {
      const char* val = lua_tostring(L, 3);
      size_t len = strlen(val);
      if (len > 1 && val[len - 1] == '%') {
        s.height = Length::Percent(static_cast<f32>(atof(val)));
      } else {
        s.height = Length::Px(static_cast<f32>(atof(val)));
      }
    } else {
      s.height = Length::Px(static_cast<f32>(luaL_checknumber(L, 3)));
    }
  } else if (strcmp(prop, "selected") == 0) {
    if (auto* dd = dynamic_cast<Dropdown*>(w)) {
      dd->set_selected_index(static_cast<i32>(luaL_checkinteger(L, 3)));
      return 0;
    }
  } else if (strcmp(prop, "checked") == 0) {
    if (auto* cb = dynamic_cast<Checkbox*>(w)) {
      cb->set_checked(lua_toboolean(L, 3));
      return 0;
    }
  } else if (strcmp(prop, "value") == 0) {
    if (auto* sl = dynamic_cast<Slider*>(w)) {
      sl->set_value(static_cast<f32>(luaL_checknumber(L, 3)));
      return 0;
    }
  } else if (strcmp(prop, "text") == 0) {
    if (auto* text = dynamic_cast<Text*>(w)) {
      text->set_text(luaL_checkstring(L, 3));
      w->ClearAnimationStyle();
      return 0;
    }
    if (auto* btn = dynamic_cast<Button*>(w)) {
      btn->set_label(luaL_checkstring(L, 3));
      w->ClearAnimationStyle();
      return 0;
    }
  }

  w->set_style(s);
  // Cancel any active CSS transition so the scripted change is visible
  // immediately. Without this, animation_style_ overrides the base style
  // and the change appears to not take effect until the transition ends.
  w->ClearAnimationStyle();
  return 0;
}

int ScriptRuntime::Impl::LuaUguiLog(lua_State* L) {
  const char* msg = luaL_checkstring(L, 1);
  std::printf("[ugui/lua] %s\n", msg);
  return 0;
}

}  // namespace ugui
