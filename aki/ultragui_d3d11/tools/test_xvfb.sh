#!/usr/bin/env bash
# test_xvfb.sh [akiko test args]: runs the package's tests under Xvfb on Mesa's
# software Vulkan (lavapipe, under DXVK-native), where d3d11_rhi_scene_test and
# d3d11_draw_data_test get a window and a device (without one they skip). Run from anywhere; it enters the dev shell.
set -eu
root=$(cd "$(dirname "$0")/../../.." && pwd)
mesa=$(nix build --no-link --print-out-paths --inputs-from "$root" nixpkgs#mesa | tail -1)
export VK_ICD_FILENAMES=$(ls "$mesa"/share/vulkan/icd.d/lvp_icd.*.json)
export LIBGL_ALWAYS_SOFTWARE=1 DXVK_WSI_DRIVER=GLFW
export LD_LIBRARY_PATH="$mesa/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec xvfb-run -a -s "-screen 0 1280x720x24" \
  nix develop "$root" -c bash -c 'cd "$1" && shift && akiko test "$@"' test_xvfb "$root/aki/ultragui_d3d11" "$@"
