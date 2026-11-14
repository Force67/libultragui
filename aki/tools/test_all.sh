#!/usr/bin/env bash
# test_all.sh [package...]: `akiko check`, `akiko check --target
# x86_64-w64-windows-gnu` and `akiko test` for every aki package (every
# directory under aki/ with an aki.toml: the packages, the examples and the
# golden runner), then a summary. The tests run under Xvfb on Mesa's software
# renderers (lavapipe for Vulkan, DXVK-native and vkd3d; llvmpipe for OpenGL),
# so the windowed backends' tests get a display and a device and run for real
# instead of skipping; their output says so if the machine cannot provide one.
# Run it from anywhere; it enters the nix dev shell itself. Names (ultragui_vulkan,
# or paths) limit it to those packages. Logs go to $TEST_ALL_LOGS
# (default /tmp/aki-test-all); the exit status is 1 if anything failed.
#   aki/tools/test_all.sh
#   aki/tools/test_all.sh ultragui_d3d12 ultragui_opengl
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)

if [ -z "${TEST_ALL_INNER:-}" ]; then
  mesa=$(nix build --no-link --print-out-paths --inputs-from "$root" nixpkgs#mesa | tail -1)
  export VK_ICD_FILENAMES=$(ls "$mesa"/share/vulkan/icd.d/lvp_icd.*.json)
  export LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa DXVK_WSI_DRIVER=GLFW
  export LD_LIBRARY_PATH="$mesa/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  export TEST_ALL_INNER=1
  exec xvfb-run -a -s "-screen 0 1280x720x24" nix develop "$root" -c "$0" "$@"
fi

logs=${TEST_ALL_LOGS:-/tmp/aki-test-all}
mkdir -p "$logs"
cd "$root/aki"
if [ $# -gt 0 ]; then
  pkgs=()
  for a in "$@"; do
    if [ -f "$a/aki.toml" ]; then pkgs+=("$(cd "$a" && pwd)"); else pkgs+=("$root/aki/$a"); fi
  done
else
  # the core first, the rest in path order
  pkgs=("$root/aki/ultragui")
  while IFS= read -r f; do
    d=$(dirname "$f")
    [ "$d" = "$root/aki/ultragui" ] || pkgs+=("$d")
  done < <(find "$root/aki" -name aki.toml -not -path '*/build/*' | sort)
fi

fails=0
rows=()
for d in "${pkgs[@]}"; do
  name=${d#"$root"/aki/}
  log="$logs/${name//\//_}"
  chk=-; win=-; tst=-
  if (cd "$d" && akiko check >"$log.check.log" 2>&1); then chk=ok; else chk=FAIL; fi
  if (cd "$d" && akiko check --target x86_64-w64-windows-gnu >"$log.windows.log" 2>&1); then win=ok; else win=FAIL; fi
  if ls "$d"/tests/*_test.aki >/dev/null 2>&1; then
    (cd "$d" && akiko test >"$log.test.log" 2>&1); rc=$?
    # akiko test prints "N passed, M failed"
    sum=$(grep -E '^[0-9]+ passed, [0-9]+ failed' "$log.test.log" | tail -1)
    # a test that cannot run prints "..., skipped", "...: skipped" or
    # "..., skipping" (not "skipped: 0", a statistic)
    skipped=$(grep -ciE '(, |: )skip(ped|ping)\b' "$log.test.log")
    [ "$skipped" -gt 0 ] && skipped=", $skipped skipped" || skipped=""
    if [ $rc -eq 0 ]; then tst="ok (${sum:-?}$skipped)"; else tst="FAIL (${sum:-no summary}; $log.test.log)"; fi
  else
    tst="no tests"
  fi
  [ "$chk" = ok ] && [ "$win" = ok ] && case $tst in FAIL*) false;; *) true;; esac || fails=$((fails + 1))
  rows+=("$(printf '%-34s check %-4s windows %-4s tests %s' "$name" "$chk" "$win" "$tst")")
  echo "${rows[-1]}"
done

echo
echo "== summary =="
printf '%s\n' "${rows[@]}"
if [ $fails -eq 0 ]; then echo "all ${#pkgs[@]} packages passed"; exit 0; fi
echo "$fails of ${#pkgs[@]} packages failed (logs in $logs)"
exit 1
