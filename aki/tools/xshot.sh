#!/usr/bin/env bash
# xshot.sh <out.png> <seconds> <command...>: run a windowed ultragui program
# under Xvfb on Mesa's software renderers (lavapipe for Vulkan, and through it
# DXVK and vkd3d; llvmpipe for OpenGL), screenshot the screen after <seconds>,
# and stop the program. For checking the GPU backends on a machine without a
# display. Run it from the repository root; it enters the nix dev shell itself.
set -eu
out=$1; secs=$2; shift 2
root=$(cd "$(dirname "$0")/../.." && pwd)
mesa=$(nix build --no-link --print-out-paths --inputs-from "$root" nixpkgs#mesa | tail -1)
export XSHOT_OUT=$out XSHOT_SECS=$secs
export VK_ICD_FILENAMES=$(ls "$mesa"/share/vulkan/icd.d/lvp_icd.*.json)
export LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa
export LD_LIBRARY_PATH="$mesa/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec xvfb-run -a -s "-screen 0 1280x720x24" \
  nix develop "$root" -c bash -c '
    "$@" & pid=$!
    sleep "$XSHOT_SECS"
    import -window root "$XSHOT_OUT"
    kill $pid 2>/dev/null; wait $pid 2>/dev/null; true' xshot "$@"
