#include <ultragui/platform/platform.h>

// Optional default implementation of Platform::OpenURL.
//
// Compiled when the ULTRAGUI_DEFAULT_OPEN_URL CMake option is ON (the default),
// so URLs open in the system browser out of the box. Turn the option OFF and
// link your own ugui::Platform::OpenURL to route URL handling through the host
// application instead.

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ugui {

void Platform::OpenURL(const char* url) {
  if (!url || !*url) return;
#if defined(_WIN32)
  ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#else
#if defined(__APPLE__)
  const char* launcher = "open";
#else
  const char* launcher = "xdg-open";
#endif
  // Double-fork so the launcher reparents to init: no zombie is left behind and
  // the browser keeps running independently of this process.
  pid_t pid = fork();
  if (pid == 0) {
    if (fork() == 0) {
      execlp(launcher, launcher, url, static_cast<char*>(nullptr));
      _exit(127);  // exec failed
    }
    _exit(0);
  } else if (pid > 0) {
    waitpid(pid, nullptr, 0);  // reap the short-lived intermediate child
  }
#endif
}

}  // namespace ugui
