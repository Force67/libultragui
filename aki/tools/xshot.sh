#!/usr/bin/env bash
# xshot.sh <out.png> <seconds> <command...>: run a windowed ultragui program
# under Xvfb on Mesa's software renderers (lavapipe for Vulkan, and through it
# DXVK and vkd3d; llvmpipe for OpenGL), screenshot the screen after <seconds>,
# and stop the program. For checking the GPU backends on a machine without a
# display. Run it from the repository root; it enters the nix dev shell itself.
#
# XSHOT_KEYS="2 space click:449,645" presses those keys (xdotool key names)
# in the window, one after another, starting XSHOT_KEYS_AT seconds after
# launch (default: half of <seconds>); each is held 0.5 s so programs that
# poll glfwGetKey see it. `click:X,Y` moves the pointer there and clicks the
# left button (held 0.3 s).
set -eu
out=$1; secs=$2; shift 2
root=$(cd "$(dirname "$0")/../.." && pwd)
mesa=$(nix build --no-link --print-out-paths --inputs-from "$root" nixpkgs#mesa | tail -1)
export XSHOT_OUT=$out XSHOT_SECS=$secs
export XSHOT_KEYS=${XSHOT_KEYS:-} XSHOT_KEYS_AT=${XSHOT_KEYS_AT:-$((secs / 2))} XSHOT_XDOTOOL=
if [ -n "$XSHOT_KEYS" ]; then
  XSHOT_XDOTOOL=$(nix build --no-link --print-out-paths --inputs-from "$root" nixpkgs#xdotool | tail -1)/bin/xdotool
fi
export VK_ICD_FILENAMES=$(ls "$mesa"/share/vulkan/icd.d/lvp_icd.*.json)
export LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa
export LD_LIBRARY_PATH="$mesa/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec xvfb-run -a -s "-screen 0 1280x720x24" \
  nix develop "$root" -c bash -c '
    set -eu
    # Never leave an older image looking like the result of this run.
    rm -f -- "$XSHOT_OUT"
    "$@" & pid=$!
    cleanup() {
      rc=$?
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
      if [ "$rc" -ne 0 ]; then rm -f -- "$XSHOT_OUT"; fi
    }
    trap cleanup EXIT
    require_running() {
      if ! kill -0 "$pid" 2>/dev/null; then
        rc=0
        wait "$pid" || rc=$?
        echo "xshot: application exited before capture (status $rc)" >&2
        exit 1
      fi
    }
    if [ -n "$XSHOT_KEYS" ]; then
      sleep "$XSHOT_KEYS_AT"
      for k in $XSHOT_KEYS; do
        case "$k" in
          click:*) xy=${k#click:}
                   "$XSHOT_XDOTOOL" mousemove "${xy%,*}" "${xy#*,}"; sleep 0.5
                   "$XSHOT_XDOTOOL" mousedown 1; sleep 0.3; "$XSHOT_XDOTOOL" mouseup 1; sleep 1 ;;
          *) "$XSHOT_XDOTOOL" keydown "$k"; sleep 0.5; "$XSHOT_XDOTOOL" keyup "$k"; sleep 1 ;;
        esac
      done
      sleep "$((XSHOT_SECS - XSHOT_KEYS_AT))"
    else
      sleep "$XSHOT_SECS"
    fi
    require_running
    import -window root "$XSHOT_OUT"
    require_running' xshot "$@"
