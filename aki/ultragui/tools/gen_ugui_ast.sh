#!/usr/bin/env bash
# Regenerates tests/data/ugui_ast/expected.txt: the C++ parser's AST dump of
# every examples/**/*.ugui and tests/data/ugui/**/*.ugui (see dump_ugui.cc), and
# the fuzz cases' dump hashes (tests/data/ugui_fuzz, cases from gen_ugui_fuzz.py).
# Run from the repository root:  nix develop -c aki/ultragui/tools/gen_ugui_ast.sh
set -euo pipefail
root=$(pwd)
bin=$(mktemp -d)/dump_ugui
clang++ -std=c++20 -O1 -I. aki/ultragui/tools/dump_ugui.cc ugui/idl/parser.cc ugui/core/from_chars.cc -o "$bin"
{ find "$root/examples" -name '*.ugui' | sort; find "$root/aki/ultragui/tests/data/ugui" -name '*.ugui' | sort; } \
  | xargs "$bin" --root "$root" > aki/ultragui/tests/data/ugui_ast/expected.txt
wc -l aki/ultragui/tests/data/ugui_ast/expected.txt
python3 aki/ultragui/tools/gen_ugui_fuzz.py
"$bin" --cases aki/ultragui/tests/data/ugui_fuzz/cases.txt > aki/ultragui/tests/data/ugui_fuzz/expected.txt
wc -l aki/ultragui/tests/data/ugui_fuzz/expected.txt
