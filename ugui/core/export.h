#ifndef ULTRAGUI_CORE_EXPORT_H_
#define ULTRAGUI_CORE_EXPORT_H_

// Public-API visibility marker for the widget free-function API.
//
// STATIC by default: UGUI_API expands to nothing. For a shared build, CMake
// defines UGUI_SHARED for consumers and UGUI_BUILD while compiling the
// library, so annotated symbols export/import correctly.
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
