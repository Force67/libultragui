#!/usr/bin/env bash
# run.sh <c++ runner> <aki runner> [scenario...]: play scenarios through both
# runners from the repository root and compare the dumps. Exit 1 on any
# difference.
set -u
cpp=$1; aki=$2; shift 2
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=${GOLDEN_OUT:-/tmp/ugui-golden-out}
mkdir -p "$out"
cd "$root"
if [ $# -eq 0 ]; then set -- "$here"/scenarios/*.txt; fi
fail=0
for s in "$@"; do
  n=$(basename "$s" .txt)
  "$cpp" "$s" "$out/$n.cpp.bin" >"$out/$n.cpp.log" 2>&1 || { echo "FAIL $n: C++ runner failed"; fail=1; continue; }
  "$aki" "$s" "$out/$n.aki.bin" >"$out/$n.aki.log" 2>&1 || { echo "FAIL $n: aki runner failed (see $out/$n.aki.log)"; fail=1; continue; }
  if r=$(python3 "$here/compare.py" "$out/$n.cpp.bin" "$out/$n.aki.bin" --max 8); then
    echo "PASS $n: ${r#same: }"
  else
    echo "FAIL $n"; echo "$r" | sed 's/^/    /'; fail=1
  fi
done
exit $fail
