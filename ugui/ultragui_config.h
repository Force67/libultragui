#ifndef ULTRAGUI_ULTRAGUI_CONFIG_H_
#define ULTRAGUI_ULTRAGUI_CONFIG_H_

/// \file ultragui_config.h
/// \brief Compile-time feature switches for libultragui (imgui imconfig.h-style).
///
/// Every optional subsystem is gated by a macro that is either 1 (enabled) or
/// 0 (disabled). Override any of them in one of three ways, in order of
/// precedence:
///
///   1. Define on the compiler command line: -DULTRAGUI_VIDEO=0
///   2. Point ULTRAGUI_USER_CONFIG at your own header:
///        -DULTRAGUI_USER_CONFIG="\"my_app/ugui_features.h\""
///   3. Edit the defaults below.
///
/// Anything left untouched falls back to the defaults here. When building with
/// the bundled CMake, its option(...) switches define these macros for you, so
/// the built library and these headers always agree.
///
/// Note: this is separate from core/config.h, which customizes container and
/// allocator *types* via ULTRAGUI_CUSTOM_CONFIG.

#if defined(ULTRAGUI_USER_CONFIG)
#include ULTRAGUI_USER_CONFIG
#endif

// --- Optional subsystems --------------------------------------------------

#ifndef ULTRAGUI_LUA
#define ULTRAGUI_LUA 1  // Built-in Lua scripting runtime
#endif

#ifndef ULTRAGUI_AUDIO
#define ULTRAGUI_AUDIO 1  // Audio engine (miniaudio)
#endif

#ifndef ULTRAGUI_LOTTIE
#define ULTRAGUI_LOTTIE 1  // Lottie animation playback (rlottie)
#endif

#ifndef ULTRAGUI_VIDEO
#define ULTRAGUI_VIDEO 1  // MPEG-1 video playback (pl_mpeg)
#endif

#ifndef ULTRAGUI_DEFAULT_OPEN_URL
#define ULTRAGUI_DEFAULT_OPEN_URL 1  // Built-in Platform::OpenURL implementation
#endif

// --- Rendering backends ---------------------------------------------------
// Vulkan is the default. Enable at most one alternative backend.

#ifndef ULTRAGUI_BACKEND_VULKAN
#define ULTRAGUI_BACKEND_VULKAN 1
#endif

#ifndef ULTRAGUI_BACKEND_D3D12
#define ULTRAGUI_BACKEND_D3D12 0
#endif

#ifndef ULTRAGUI_BACKEND_D3D11
#define ULTRAGUI_BACKEND_D3D11 0
#endif

#ifndef ULTRAGUI_BACKEND_OPENGL
#define ULTRAGUI_BACKEND_OPENGL 0
#endif

#endif  // ULTRAGUI_ULTRAGUI_CONFIG_H_
