#!/usr/bin/env bash
# xshot_fixed.sh <out.png> <seconds> <fixed_time> <command...>: xshot.sh with
# the program's GLFW clock driven by glfw_fixed_time.c (time advances 1/60 s
# per glfwPollEvents and stops at <fixed_time>), so screenshots of animated
# programs taken after they reach <fixed_time> are comparable; Lua's
# math.random is seeded (42) as well. Run from the
# repository root.
set -eu
out=$1; secs=$2; fixed=$3; shift 3
here=$(cd "$(dirname "$0")" && pwd)
so=${TMPDIR:-/tmp}/glfw_fixed_time.so
if [ ! -f "$so" ] || [ "$here/glfw_fixed_time.c" -nt "$so" ]; then
  nix develop "$here/../.." -c cc -shared -fPIC -O2 -o "$so" "$here/glfw_fixed_time.c" -ldl
fi
exec "$here/xshot.sh" "$out" "$secs" env LD_PRELOAD="$so" UGUI_FIXED_TIME="$fixed" "$@"
