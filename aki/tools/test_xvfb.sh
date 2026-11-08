#!/usr/bin/env bash
# test_xvfb.sh <package> [akiko test args]: runs a package's `akiko test` under
# Xvfb on Mesa's software renderers (lavapipe for Vulkan, and through it DXVK-native
# and vkd3d; llvmpipe for OpenGL), where the windowed tests get a display and a
# device (without one they skip and pass). <package> is a directory under aki/
# (ultragui_d3d12) or a path. Run from anywhere; it enters the dev shell itself.
#   aki/tools/test_xvfb.sh ultragui_d3d11
#   aki/tools/test_xvfb.sh ultragui_d3d12 tests/rhi_test.aki
set -eu
[ $# -ge 1 ] || { sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
root=$(cd "$(dirname "$0")/../.." && pwd)
pkg=$1; shift
[ -d "$pkg" ] || pkg="$root/aki/$pkg"
pkg=$(cd "$pkg" && pwd)
mesa=$(nix build --no-link --print-out-paths --inputs-from "$root" nixpkgs#mesa | tail -1)
export VK_ICD_FILENAMES=$(ls "$mesa"/share/vulkan/icd.d/lvp_icd.*.json)
export LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa DXVK_WSI_DRIVER=GLFW
export LD_LIBRARY_PATH="$mesa/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec xvfb-run -a -s "-screen 0 1280x720x24" \
  nix develop "$root" -c bash -c 'cd "$1" && shift && akiko test "$@"' test_xvfb "$pkg" "$@"
