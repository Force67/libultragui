/* glfw_fixed_time.c: an LD_PRELOAD shim that makes a GLFW program's clock a
 * function of its frame count, for comparing screenshots of animated
 * programs (aki/tools/xshot_fixed.sh). glfwGetTime returns
 * min(polls * UGUI_TIME_STEP, UGUI_FIXED_TIME), where polls counts
 * glfwPollEvents calls (one per frame in ultragui's UIContext): every frame
 * sees the same dt sequence, and once the program reaches UGUI_FIXED_TIME
 * its clock stops, so the C++ program and its aki port settle on the same
 * frame whatever their speed. UGUI_TIME_STEP defaults to 1/60 s.
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

void glfwPollEvents(void) {
  static void (*real)(void) = 0;
  if (!real) real = (void (*)(void))dlsym(RTLD_NEXT, "glfwPollEvents");
  ++g_polls;
  if (real) real();
}
