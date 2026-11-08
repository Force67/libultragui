#!/usr/bin/env python3
"""Writes tests/data/anim_fuzz/cases.txt: mutations of the .uganim documents
(deleted/duplicated/truncated spans, replaced numbers and tokens), separated by
"=====CASE=====" lines. tools/gen_anim_reference.cc records what the C++ makes
of them (FUZZ lines in tests/data/anim_reference.txt). Deterministic."""
import random, glob, os, sys, re

root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
rng = random.Random(7771)
files = sorted(glob.glob(os.path.join(root, "examples", "anim_demo", "anims", "*.uganim")))
files += sorted(glob.glob(os.path.join(root, "aki", "ultragui", "tests", "data", "anim", "*.uganim")))
repl = ['0', '1', '-1', '0.5', '1e9', '-0', '100', 'null', 'true', '"x"', '[]', '{}', '"#ff00ff"', '"none"', '"circle"',
        '"path"', '"group"', '"ease-in"', '2.5e-3', '1e400', '"M 0 0 L 10 10"', '""']

def mutate(s):
    for _ in range(rng.randint(1, 3)):
        op = rng.choices(range(6), weights=[1, 1, 1, 5, 5, 1])[0]
        i = rng.randint(0, len(s) - 1)
        j = min(len(s), i + rng.randint(1, 40))
        if op == 0:
            s = s[:i] + s[j:]
        elif op == 1:
            s = s[:j] + s[i:j] + s[j:]
        elif op == 2:
            s = s[:i + 1]
        elif op in (3, 4):
            # replace a number or string token by something else
            toks = list(re.finditer(r'-?\d+\.?\d*(?:[eE][-+]?\d+)?|"[^"]*"', s))
            if toks:
                m = rng.choice(toks)
                s = s[:m.start()] + rng.choice(repl) + s[m.end():]
        else:
            s = s[:i] + rng.choice(['{', '}', '[', ']', ',', ':', '"', '\\', ' ']) + s[j:]
    return s

cases = []
for p in files:
    text = open(p, encoding="utf-8").read()
    for k in range(100):
        cases.append(text if k == 0 else mutate(text))
cases = [c for c in cases if "=====CASE=====" not in c and all(ord(ch) < 128 for ch in c)]
out = os.path.join(root, "aki", "ultragui", "tests", "data", "anim_fuzz")
os.makedirs(out, exist_ok=True)
with open(os.path.join(out, "cases.txt"), "w", encoding="utf-8", newline="\n") as f:
    f.write("\n=====CASE=====\n".join(cases))
print(len(cases), "cases", file=sys.stderr)
