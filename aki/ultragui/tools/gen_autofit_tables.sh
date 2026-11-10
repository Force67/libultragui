#!/usr/bin/env bash
# Regenerates src/font/autofit/tables.aki. Run inside the repository's nix
# dev shell.
set -euo pipefail
pkg=$(cd "$(dirname "$0")/.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
tar -xf "$(nix build --inputs-from "$pkg/../.." nixpkgs#freetype.src --no-link --print-out-paths)" -C "$build"
src="$build/freetype-2.14.3"
sed -n '/typedef struct  AF_AdjustmentDatabaseEntry_/,/^  };/p' "$src/src/autofit/afadjust.c" > "$build/adjust_db.h"
clang -std=c11 -O1 -DFT2_BUILD_LIBRARY $(pkg-config --cflags freetype2) -I"$src/include" -I"$src/src/autofit" -I"$build" \
  "$pkg/tools/gen_autofit_tables.c" $(pkg-config --libs harfbuzz) -o "$build/gen"
"$build/gen" > "$pkg/src/font/autofit/tables.aki"
