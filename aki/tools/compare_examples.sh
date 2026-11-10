#!/usr/bin/env bash
# compare_examples.sh [case-filter]: build the C++ examples (two reference
# builds: Vulkan, and OpenGL for embed_gl) and the aki examples, screenshot
# both under Xvfb with the fixed GLFW clock (xshot_fixed.sh), and compare
# the screenshots pixel by pixel (compare_shots.sh); the previewer's
# --screenshot PNGs are compared byte for byte. The matrix runs the example
# UIs with several fonts (TrueType, CFF OpenType, collections, embedded
# bitmap strikes, other scripts), so it checks the text engine end to end on
# real GPU drivers (lavapipe, llvmpipe). Run from anywhere; prints one line
# per case and a summary; exit status 1 if any case differs or fails.
#   aki/tools/compare_examples.sh            # everything
#   aki/tools/compare_examples.sh showcase   # cases whose name matches
# Environment: REF_EX, REF_GL (reference build dirs, default
# /tmp/ugui-ref-ex, /tmp/ugui-ref-gl), SHOTS (output dir, default
# /tmp/aki-compare-examples), SKIP_BUILD=1 to reuse existing builds.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
filter=${1:-}
# Per-checkout default build dirs: a CMake cache from another worktree would
# make the configure step fail.
tag=$(printf '%s' "$root" | cksum | cut -d' ' -f1)
ref_ex=${REF_EX:-/tmp/ugui-ref-ex-$tag}
ref_gl=${REF_GL:-/tmp/ugui-ref-gl-$tag}
shots=${SHOTS:-/tmp/aki-compare-examples}
mkdir -p "$shots"

if [ -z "${SKIP_BUILD:-}" ]; then
  set -o pipefail
  nix develop -c bash -c "
    set -e
    cmake -S . -B '$ref_ex' -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null && ninja -C '$ref_ex' >/dev/null
    cmake -S . -B '$ref_gl' -G Ninja -DCMAKE_BUILD_TYPE=Release -DULTRAGUI_BACKEND_OPENGL=ON \
      -DULTRAGUI_BACKEND_VULKAN=OFF -DULTRAGUI_BUILD_SHADERS=ON -DULTRAGUI_BUILD_TESTS=OFF >/dev/null && ninja -C '$ref_gl' >/dev/null
    for d in aki/examples/*/; do (cd \"\$d\" && akiko build >/dev/null) || { echo \"build failed: \$d\" >&2; exit 1; }; done
  " 2>&1 | { grep -v "Git tree\|sysprof\|Perhaps\|PKG_CONFIG\|required by\|dev shell" || true; } ||
    { echo "compare_examples: build failed" >&2; exit 1; }
  set +o pipefail
fi

cpp() { # the C++ binary for an example name
  local b
  b=$(find "$1" -type f -executable -name "$2" -not -path '*/CMakeFiles/*' 2>/dev/null | head -1)
  echo "$b"
}

dejavu=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
fonts=(
  "liberation=/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
  "cantarell_otf=/usr/share/fonts/opentype/cantarell/Cantarell-Regular.otf"
  "noto_cjk_ttc=/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
  "uming_ttc=/usr/share/fonts/truetype/arphic/uming.ttc"
  "lohit_devanagari=/usr/share/fonts/truetype/lohit-devanagari/Lohit-Devanagari.ttf"
)

# Runs a command under Xvfb on Mesa's software drivers, as xshot.sh does.
mesa=$(nix build --no-link --print-out-paths --inputs-from "$root" nixpkgs#mesa | tail -1)
headless() {
  VK_ICD_FILENAMES=$(ls "$mesa"/share/vulkan/icd.d/lvp_icd.*.json) LIBGL_ALWAYS_SOFTWARE=1 \
    __GLX_VENDOR_LIBRARY_NAME=mesa LD_LIBRARY_PATH="$mesa/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    xvfb-run -a -s "-screen 0 1280x720x24" nix develop "$root" -c "$@"
}

pass=0; fail=0; failed=()
run_case() { # name secs fixed_time cpp_cmd... -- aki_cmd...
  local name=$1 secs=$2 fixed=$3; shift 3
  [ -n "$filter" ] && [[ "$name" != *"$filter"* ]] && return
  local c=() a=() seen=0
  for x in "$@"; do
    if [ "$x" = "--" ]; then seen=1; continue; fi
    if [ $seen = 0 ]; then c+=("$x"); else a+=("$x"); fi
  done
  rm -f "$shots/$name.cpp.png" "$shots/$name.aki.png"
  local out=
  if [ -z "${c[0]}" ] || [ ! -x "${c[0]}" ]; then
    echo "FAIL $name (no C++ binary)"; fail=$((fail + 1)); failed+=("$name"); return
  fi
  if aki/tools/xshot_fixed.sh "$shots/$name.cpp.png" "$secs" "$fixed" "${c[@]}" >"$shots/$name.cpp.log" 2>&1 &&
     aki/tools/xshot_fixed.sh "$shots/$name.aki.png" "$secs" "$fixed" "${a[@]}" >"$shots/$name.aki.log" 2>&1 &&
     out=$(aki/tools/compare_shots.sh "$shots/$name.aki.png" "$shots/$name.cpp.png" "$shots/$name.diff.png" 2>&1); then
    echo "PASS $name"; pass=$((pass + 1))
  else
    echo "FAIL $name ${out:-capture failed, see $shots/$name.*.log}"; fail=$((fail + 1)); failed+=("$name")
  fi
}
preview_case() { # name file.ugui [args...]
  local name=$1; shift
  [ -n "$filter" ] && [[ "$name" != *"$filter"* ]] && return
  local c a
  c=$(cpp "$ref_gl" ugui_preview); [ -n "$c" ] || c=$(cpp "$ref_ex" ugui_preview)
  a=aki/examples/preview/build/ugui_preview
  rm -f "$shots/$name.cpp.png" "$shots/$name.aki.png"
  if headless "$c" "$@" --screenshot "$shots/$name.cpp.png" >"$shots/$name.cpp.log" 2>&1 &&
     headless "$a" "$@" --screenshot "$shots/$name.aki.png" >"$shots/$name.aki.log" 2>&1 &&
     cmp -s "$shots/$name.cpp.png" "$shots/$name.aki.png"; then
    echo "PASS $name"; pass=$((pass + 1))
  else
    echo "FAIL $name (PNG files differ or a run failed, see $shots/$name.*.log)"; fail=$((fail + 1)); failed+=("$name")
  fi
}

ex=aki/examples
run_case demo 15 1.5 "$(cpp "$ref_ex" ultragui_demo)" -- $ex/demo/build/ultragui_demo
run_case anim_demo 15 1.3 "$(cpp "$ref_ex" ultragui_anim_demo)" "$dejavu" -- $ex/anim_demo/build/ultragui_anim_demo "$dejavu"
run_case lottie_demo 15 1.3 "$(cpp "$ref_ex" ultragui_lottie_demo)" "$dejavu" -- $ex/lottie_demo/build/ultragui_lottie_demo "$dejavu"
for scene in dashboard neon glass terminal nerv cosmos rpg_menu meridian; do
  run_case "showcase_${scene}" 15 2.0 "$(cpp "$ref_ex" ultragui_showcase)" "$scene" "$dejavu" -- \
    $ex/showcase/build/ultragui_showcase "$scene" "$dejavu"
done
for f in "${fonts[@]}"; do
  key=${f%%=*}; path=${f#*=}
  for scene in dashboard rpg_menu terminal; do
    run_case "showcase_${scene}_${key}" 15 2.0 "$(cpp "$ref_ex" ultragui_showcase)" "$scene" "$path" -- \
      $ex/showcase/build/ultragui_showcase "$scene" "$path"
  done
  run_case "game_overlay_${key}" 15 2.0 "$(cpp "$ref_ex" ultragui_game_overlay)" "$path" -- \
    $ex/game_overlay/build/ultragui_game_overlay "$path"
  run_case "anim_demo_${key}" 15 1.3 "$(cpp "$ref_ex" ultragui_anim_demo)" "$path" -- \
    $ex/anim_demo/build/ultragui_anim_demo "$path"
done
run_case game_overlay 15 2.0 "$(cpp "$ref_ex" ultragui_game_overlay)" "$dejavu" -- $ex/game_overlay/build/ultragui_game_overlay "$dejavu"
run_case video_demo 15 2.0 "$(cpp "$ref_ex" ultragui_video_demo)" "$dejavu" -- $ex/video_demo/build/ultragui_video_demo "$dejavu"
run_case embed_gl 15 2.0 "$(cpp "$ref_gl" ultragui_embed_gl)" -- $ex/embed_gl/build/ultragui_embed_gl
run_case embed_opengl3 15 2.0 "$(cpp "$ref_ex" ultragui_embed_opengl3)" -- $ex/embed_opengl3/build/ultragui_embed_opengl3
run_case embed_vulkan 15 2.0 "$(cpp "$ref_ex" ultragui_embed_vulkan)" -- $ex/embed_vulkan/build/ultragui_embed_vulkan
for u in examples/showcase/scenes/dashboard.ugui examples/showcase/scenes/neon.ugui examples/game_overlay/hud.ugui examples/showcase/ui.ugui; do
  n=$(basename "$u" .ugui)
  lua=${u%.ugui}.lua
  if [ -f "$lua" ]; then
    preview_case "preview_$n" "$u" --script "$lua" --size 1024x640 --frames 40 --bg '#102030'
  else
    preview_case "preview_$n" "$u" --size 1024x640 --frames 40 --bg '#102030'
  fi
done
for f in "$dejavu" "${fonts[@]}"; do
  key=${f%%=*}; path=${f#*=}; [ "$key" = "$f" ] && key=dejavu
  preview_case "preview_text_matrix_${key}" aki/tools/golden/extra/text_matrix.ugui --font "$path" --size 1280x720 --frames 10
done

echo "== $pass passed, $fail failed${failed:+: ${failed[*]}}"
[ "$fail" -eq 0 ]
