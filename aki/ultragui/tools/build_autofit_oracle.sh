#!/usr/bin/env bash
# Run inside the repository's nix dev shell. Private source is needed only
# by the test oracle; the Aki library never calls or links these routines.
set -euo pipefail
pkg=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-"$pkg/build/autofit_oracle"}
mkdir -p "$build"
source_archive=$(nix build --inputs-from "$pkg/../.." nixpkgs#freetype.src --no-link --print-out-paths)
if [[ ! -d "$build/freetype-2.14.3" ]]; then
  tar -xf "$source_archive" -C "$build"
fi
src="$build/freetype-2.14.3"
oracle_tmp="$build/autofit_oracle.$$"
trap 'rm -f "$oracle_tmp"' EXIT
clang -std=c11 -O2 -ffunction-sections -fdata-sections -DFT2_BUILD_LIBRARY \
  -I"$src/include" -I"$src/src/autofit" $(pkg-config --cflags harfbuzz freetype2) \
  "$pkg/tools/autofit_oracle.c" "$src/src/autofit/afhints.c" \
  "$src/src/base/ftcalc.c" "$src/src/base/ftutil.c" \
  "$src/src/base/fthash.c" "$src/src/autofit/afadjust.c" \
  -Wl,--gc-sections $(pkg-config --libs freetype2 harfbuzz) -o "$oracle_tmp"
mv "$oracle_tmp" "$build/autofit_oracle"
