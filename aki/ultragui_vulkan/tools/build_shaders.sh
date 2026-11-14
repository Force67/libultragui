#!/usr/bin/env bash
# Compiles the repository's GLSL shaders (shaders/*.{vert,frag}) to the SPIR-V
# copies in aki/ultragui_vulkan/shaders/ that the `embed_shaders` feature
# @embed-s, with the same command CMakeLists.txt runs for its shader_dir
# (glslc <src> -o <name>.spv). Run from the repository root in the dev shell
# whenever a shader changes:
#   nix develop -c aki/ultragui_vulkan/tools/build_shaders.sh
set -eu
out=aki/ultragui_vulkan/shaders
for s in quad.vert quad.frag text.vert text.frag video.vert video.frag; do
  glslc "shaders/$s" -o "$out/$s.spv"
done
