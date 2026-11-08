#!/usr/bin/env bash
# Builds rhi_scene.cc against a C++ reference build of libultragui (one made
# with the default options: GLFW platform, Vulkan RHI, shaders). Run from the
# repository root in the dev shell:
#   nix develop -c aki/ultragui_vulkan/examples/rhi_scene/build_cpp.sh /tmp/ugui-ref
# Output: <ref-build-dir>/rhi_scene_cpp
set -eu
ref=$1
here=$(dirname "$0")
clang++ -std=c++20 -O2 -I. -o "$ref/rhi_scene_cpp" "$here/rhi_scene.cc" \
  "$ref/libultragui.a" "$ref/third_party/yoga/yoga/libyogacore.a" \
  -lglfw -lvulkan -lfreetype -lharfbuzz -llua -lpthread -lm -ldl
