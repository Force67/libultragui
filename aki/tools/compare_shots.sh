#!/usr/bin/env bash
# compare_shots.sh <a.png> <b.png> [diff.png]: say whether two xshot.sh
# screenshots are pixel-identical (prints the count of differing pixels).
# Typical use, from the repository root:
#   UGUI_SCENE_SECS=5 aki/tools/xshot.sh /tmp/aki.png 4 aki/ultragui_opengl/build/tests/gl_rhi_scene_test
#   UGUI_SCENE_SECS=5 aki/tools/xshot.sh /tmp/cpp.png 4 /tmp/rhi_scene_gl      # see tools/rhi_scene.cc
#   aki/tools/compare_shots.sh /tmp/aki.png /tmp/cpp.png
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
n=$(nix develop "$root" -c compare -metric AE "$1" "$2" "${3:-null:}" 2>&1 | tail -1 | awk '{print $1}')
if [ "$n" = "0" ]; then echo "identical: $1 $2"; else echo "DIFFERENT: $n pixels differ ($1 $2)"; exit 1; fi
