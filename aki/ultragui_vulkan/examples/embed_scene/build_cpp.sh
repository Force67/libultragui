#!/usr/bin/env bash
# Builds embed_scene.cc with ugui/backends/ugui_impl_vulkan.cc against a C++
# reference build of libultragui. Run from the repository root in the dev
# shell:
#   nix develop -c aki/ultragui_vulkan/examples/embed_scene/build_cpp.sh /tmp/ugui-ref
# Output: <ref-build-dir>/embed_scene_cpp
set -eu
ref=$1
here=$(dirname "$0")
clang++ -std=c++20 -O2 -I. -o "$ref/embed_scene_cpp" "$here/embed_scene.cc" ugui/backends/ugui_impl_vulkan.cc \
  "$ref/libultragui.a" "$ref/third_party/yoga/yoga/libyogacore.a" \
  -lglfw -lvulkan -lfreetype -lharfbuzz -llua -lpthread -lm -ldl
