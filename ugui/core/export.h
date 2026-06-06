#ifndef ULTRAGUI_CORE_EXPORT_H_
#define ULTRAGUI_CORE_EXPORT_H_

// Public-API visibility marker for the widget free-function API.
//
// The library builds STATIC by default, where UGUI_API expands to nothing. For
// a shared build the CMake target defines UGUI_SHARED for everyone and
// UGUI_BUILD while compiling the library itself, so annotated symbols are
// exported from the shared object and imported by consumers.
#if defined(UGUI_SHARED)
#if defined(_WIN32)
#if defined(UGUI_BUILD)
#define UGUI_API __declspec(dllexport)
#else
#define UGUI_API __declspec(dllimport)
#endif
#else
#define UGUI_API __attribute__((visibility("default")))
#endif
#else
#define UGUI_API
#endif

#endif  // ULTRAGUI_CORE_EXPORT_H_
