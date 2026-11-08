#!/usr/bin/env bash
# build_cpp.sh <rhi_scene|embed_scene> <ref-build-dir>: builds the C++ twin of
# an example in this directory against a C++ reference build of libultragui
# (one made with the default options: GLFW platform, Vulkan RHI, shaders). Run
# from the repository root in the dev shell:
#   nix develop -c aki/ultragui_vulkan/examples/build_cpp.sh rhi_scene /tmp/ugui-ref
# Output: <ref-build-dir>/<example>_cpp. embed_scene.cc also needs the C++
# ugui_impl_vulkan backend, which is compiled in.
set -eu
ex=$1; ref=$2
here=$(dirname "$0")
case $ex in
  rhi_scene) extra="" ;;
  embed_scene) extra="ugui/backends/ugui_impl_vulkan.cc" ;;
  *) echo "unknown example $ex (rhi_scene or embed_scene)" >&2; exit 2 ;;
esac
clang++ -std=c++20 -O2 -I. -o "$ref/${ex}_cpp" "$here/$ex/$ex.cc" $extra \
  "$ref/libultragui.a" "$ref/third_party/yoga/yoga/libyogacore.a" \
  -lglfw -lvulkan -lfreetype -lharfbuzz -llua -lpthread -lm -ldl
