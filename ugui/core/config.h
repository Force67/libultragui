#ifndef ULTRAGUI_CORE_CONFIG_H_
#define ULTRAGUI_CORE_CONFIG_H_

/// Container type customization point.
///
/// By default, container types alias to the STL. To redirect every type used
/// by the library, define ULTRAGUI_CUSTOM_CONFIG to a header path before any
/// ultragui include:
///
///   -DULTRAGUI_CUSTOM_CONFIG="my_engine/ugui_config.h"
///
/// The header must provide these aliases in namespace ugui:
///
///   template <typename T>             using Vector   = ...;
///   template <typename K, typename V> using HashMap = ...;
///   template <typename Sig>           using Function = ...;
///   template <typename T>             using Optional = ...;
///   using String = ...;
///
/// Types must be API-compatible with their STL counterparts
/// (push_back, emplace_back, operator[], find, begin/end, ...).

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
