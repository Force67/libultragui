#!/usr/bin/env python3
"""Writes tests/data/ugui_fuzz/cases.txt: small .ugui snippets and mutations of
them (deleted, duplicated and truncated spans, inserted tokens), separated by
"=====CASE=====" lines. The C++ parser's dump hashes for them are recorded with
tools/gen_ugui_ast.sh. Deterministic: rerun to regenerate the same file."""
import random, glob, os, sys

root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
rng = random.Random(20260817)
sources = sorted(glob.glob(os.path.join(root, "examples", "**", "*.ugui"), recursive=True))
sources += sorted(glob.glob(os.path.join(root, "aki", "ultragui", "tests", "data", "ugui", "*.ugui")))
sources = [p for p in sources if "errors_import" not in p]
tokens = ['{', '}', ':', ';', '@', '@media', '@keyframes', '"', '#', 'class', 'component', 'prop', '50%', '(', ')',
          '\n', '/*', '//', ':hover', '$x', '-', '1.5px', '--v', 'slot', '\\', '*/']

def snippet(text):
    lines = text.split("\n")
    n = rng.randint(3, 22)
    a = rng.randint(0, max(0, len(lines) - n))
    return "\n".join(lines[a:a + n])[:700]

def mutate(s):
    for _ in range(rng.randint(1, 3)):
        if not s:
            break
        op = rng.randint(0, 4)
        i = rng.randint(0, len(s) - 1)
        j = min(len(s), i + rng.randint(1, 30))
        if op == 0:
            s = s[:i] + s[j:]
        elif op == 1:
            s = s[:j] + s[i:j] + s[j:]
        elif op == 2:
            s = s[:i + 1]
        elif op == 3:
            s = s[:i] + " " + rng.choice(tokens) + " " + s[i:]
        else:
            s = s[:i] + rng.choice(tokens) + s[j:]
    return s

cases = []
for p in sources:
    text = open(p, encoding="utf-8").read()
    for k in range(40):
        base = snippet(text) if k % 4 else text[:700]
        cases.append(base if k < 3 else mutate(base))
cases = [c for c in cases if "=====CASE=====" not in c and "import" not in c and all(ord(ch) < 128 for ch in c)]
out = os.path.join(root, "aki", "ultragui", "tests", "data", "ugui_fuzz")
os.makedirs(out, exist_ok=True)
with open(os.path.join(out, "cases.txt"), "w", encoding="utf-8", newline="\n") as f:
    f.write("\n=====CASE=====\n".join(cases))
print(len(cases), "cases", file=sys.stderr)
