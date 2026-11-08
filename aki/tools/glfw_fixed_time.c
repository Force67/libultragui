/* glfw_fixed_time.c: an LD_PRELOAD shim that makes a GLFW program's clock a
 * function of its frame count, for comparing screenshots of animated
 * programs (aki/tools/xshot_fixed.sh). glfwGetTime returns
 * min(polls * UGUI_TIME_STEP, UGUI_FIXED_TIME), where polls counts
 * glfwPollEvents calls (one per frame in ultragui's UIContext): every frame
 * sees the same dt sequence, and once the program reaches UGUI_FIXED_TIME
 * its clock stops, so the C++ program and its aki port settle on the same
 * frame whatever their speed. UGUI_TIME_STEP defaults to 1/60 s.
 *
 * It also seeds Lua's math.random: Lua 5.4 seeds each state from the clock
 * and addresses (luai_makeseed), so a script that draws random numbers (the
 * showcase's neon, glass, terminal) never replays. luaopen_math is
 * interposed: after the real one it calls math.randomseed(UGUI_LUA_SEED,
 * default 42), as the golden runners do with ExecScript. Programs without
 * Lua never call it; the Lua API is looked up at run time.
 *
 *   cc -shared -fPIC -O2 -o /tmp/glfw_fixed_time.so glfw_fixed_time.c -ldl
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>

static unsigned long g_polls = 0;

static double env_or(const char* name, double fallback) {
  const char* s = getenv(name);
  return s ? atof(s) : fallback;
}

double glfwGetTime(void) {
  double step = env_or("UGUI_TIME_STEP", 1.0 / 60.0);
  double limit = env_or("UGUI_FIXED_TIME", 1.0);
  double t = (double)g_polls * step;
  return t < limit ? t : limit;
}

typedef struct lua_State lua_State;

int luaopen_math(lua_State* L) {
  int (*real)(lua_State*) = (int (*)(lua_State*))dlsym(RTLD_NEXT, "luaopen_math");
  int (*getfield)(lua_State*, int, const char*) =
      (int (*)(lua_State*, int, const char*))dlsym(RTLD_DEFAULT, "lua_getfield");
  void (*pushinteger)(lua_State*, long long) =
      (void (*)(lua_State*, long long))dlsym(RTLD_DEFAULT, "lua_pushinteger");
  void (*callk)(lua_State*, int, int, long, void*) =
      (void (*)(lua_State*, int, int, long, void*))dlsym(RTLD_DEFAULT, "lua_callk");
  int n = real(L);  /* leaves the math table on the stack */
  getfield(L, -1, "randomseed");
  pushinteger(L, (long long)env_or("UGUI_LUA_SEED", 42.0));
  callk(L, 1, 0, 0, 0);
  return n;
}

void glfwPollEvents(void) {
  static void (*real)(void) = 0;
  if (!real) real = (void (*)(void))dlsym(RTLD_NEXT, "glfwPollEvents");
  ++g_polls;
  if (real) real();
}
