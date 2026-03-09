#ifndef ULTRAGUI_CORE_CONFIG_H_
#define ULTRAGUI_CORE_CONFIG_H_

/// \file config.h
/// \brief Type customization point for libultragui.
///
/// By default, libultragui aliases its container and utility types to their
/// standard-library equivalents. Middleware integrators (game engines, app
/// frameworks) can redirect every allocation and type used by the library by
/// defining \c ULTRAGUI_CUSTOM_CONFIG to a header path **before** any ultragui
/// include:
///
/// \code
///   // In your build system (CMake, premake, etc.):
///   add_definitions(-DULTRAGUI_CUSTOM_CONFIG="my_engine/ugui_config.h")
/// \endcode
///
/// Your custom header must provide the following aliases inside
/// \c namespace ugui:
///
/// \code
///   namespace ugui {
///     template <typename T>         using Vector   = ...;
///     template <typename K, typename V> using HashMap = ...;
///     template <typename Sig>       using Function = ...;
///     template <typename T>         using Optional = ...;
///     using String = ...;
///   }
/// \endcode
///
/// The custom types must be API-compatible with their STL counterparts
/// (push_back, emplace_back, operator[], size, find, begin/end, etc.).

#ifdef ULTRAGUI_CUSTOM_CONFIG
#include ULTRAGUI_CUSTOM_CONFIG
#else

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ugui {

template <typename T>
using Vector = std::vector<T>;

using String = std::string;

template <typename K, typename V>
using HashMap = std::unordered_map<K, V>;

template <typename Sig>
using Function = std::function<Sig>;

template <typename T>
using Optional = std::optional<T>;

}  // namespace ugui

#endif  // ULTRAGUI_CUSTOM_CONFIG

#endif  // ULTRAGUI_CORE_CONFIG_H_
