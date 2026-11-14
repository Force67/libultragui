#!/usr/bin/env python3
"""Regenerate the small synthetic CFF fixtures (fontTools, test tooling only).
No system font outlines are copied. Each glyph exercises an operator family
or a malformed-program boundary that real installed fonts rarely contain.
"""
from pathlib import Path
from fontTools.fontBuilder import FontBuilder
from fontTools.misc.psCharStrings import T2CharString
from fontTools.cffLib import SubrsIndex

OUT = Path(__file__).resolve().parent.parent / "tests/data/cff"

def font(programs, cff2=False, regions=None, private=None):
    fb = FontBuilder(1000, isTTF=False)
    order = list(programs)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap({65 + i: n for i, n in enumerate(order[1:])})
    fb.setupHorizontalMetrics({n: (600 + i, 0) for i, n in enumerate(order)})
    fb.setupHorizontalHeader(ascent=800, descent=-200, lineGap=100)
    fb.setupNameTable({"familyName": "CFF differential fixture", "styleName": "Regular"})
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, sTypoLineGap=100, usWinAscent=900, usWinDescent=250)
    fb.setupPost()
    fb.font.recalcBBoxes = False
    fb.font.recalcTimestamp = False
    fb.font["head"].created = fb.font["head"].modified = 2082844800
    chars = {n: T2CharString(program=p) if isinstance(p, list) else T2CharString(bytecode=p) for n, p in programs.items()}
    if cff2:
        fb.setupFvar([("wght", 100, 400, 900, "Weight")], [])
        fb.setupCFF2(chars, [private or {}], regions=regions)
    else:
        fb.setupCFF("CffFixture", {"FullName": "CFF differential fixture", "FamilyName": "CFF fixture", "Weight": "Regular"}, chars, private or {})
    return fb

triangle = [100, 0, "rmoveto", 100, 300, 100, -300, "rlineto", "endchar"]
programs = {
    ".notdef": ["endchar"],
    "A": triangle,
    "acute": [0, 100, "rmoveto", 100, 100, "rlineto", "endchar"],
    "Aacute": [100, 400, 65, 194, "endchar"],
    "arithmetic": [100, 50, "add", 2, "div", 40, "sub", 10, "mul", 1, "neg", "abs", "rmoveto", 100, "dup", "rlineto", "endchar"],
    "booleans": [0, 1, "and", "not", 1, 0, "or", "eq", 100, "mul", 20, "rmoveto", 100, 100, "rlineto", "endchar"],
    "storage": [100, 0, "put", 0, "get", 100, 4, 9, "ifelse", 10, "rmoveto", 100, 100, "rlineto", "endchar"],
    "stack": [100, 200, 2, 1, "roll", "exch", 1, "index", "drop", "rmoveto", 100, 100, "rlineto", "endchar"],
    "sqrt": [200, "sqrt", 10, "mul", 10, "rmoveto", 100, 100, "rlineto", "endchar"],
    "hflex": [100, 100, "rmoveto", 30, 40, 50, 60, 70, 80, 90, "hflex", "endchar"],
    "hflex1": [100, 100, "rmoveto", 30, 40, 50, 60, 70, 80, 90, 100, 110, "hflex1", "endchar"],
    "flex": [100, 100, "rmoveto", 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 50, "flex", "endchar"],
    "flex1": [100, 100, "rmoveto", 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, "flex1", "endchar"],
    "fractional": [100.5, -100.5, "rmoveto", 99.75, 100.25, "rlineto", "endchar"],
    "unknown": bytes([139, 139, 0, 2, 17, 9, 12, 38, 14]),
    "underflow": [10, "rmoveto", "endchar"],
    "invalid_subr": [1000, "callsubr", "endchar"],
    "return_at_top": ["return"],
    "truncated_short": bytes([28, 0]),
    "truncated_fixed": bytes([255, 0, 0]),
    "implicit_end": bytes([239, 239, 21, 239, 239, 5]),
    "odd_rlineto": [0, 0, "rmoveto", 100, 100, 99, "rlineto", "endchar"],
    "index_empty": [0, "index", "endchar"],
    "get_invalid": [99, "get", "endchar"],
    "many_hints_unused": [0, 20] * 24 + ["hstem"] + ([0, 20] * 24 + ["hstem"]) * 4 + ["endchar"],
    "many_hints_drawn": [0, 20] * 24 + ["hstem"] + ([0, 20] * 24 + ["hstem"]) * 4 + triangle,
    "roll_negative": [-2, 4, "roll", "endchar"],
}
fb = font(programs, private={"BlueValues": [-20, 0, 680, 700], "OtherBlues": [-220, -200], "BlueScale": 0.039625})
fb.save(OUT / "operators.otf")

# CFF2's missing FDSelect means every glyph uses FD 0. Default weights
# for these regions are zero, and blends must leave the base operands.
programs2 = {".notdef": [], "triangle": triangle[:-1],
    "blend": [0, "vsindex", 100, 0, 200, 30, 2, "blend", "rmoveto", 100, 300, 100, -300, "rlineto"],
    "flex": programs["flex"][:-1],
    "explicit_end": bytes([14, 239, 239, 21, 11, 239, 239, 5]),
    "blend_underflow": [2, "blend"],
    "end_clears_stack": bytes([239, 239, 14, 21]),
    "late_vsindex": [100, 50, 1, "blend", 0, "vsindex"],
}
fb = font(programs2, cff2=True, regions=[{"wght": (0, 1, 1)}], private={"BlueValues": [-20, 0, 680, 700]})
fb.save(OUT / "default-cff2.otf")
print(OUT)

# Reproducible malformed-program differential cases. Avoid random (12,23),
# whose default FreeType driver seed is deliberately process-dependent.
import random
rng = random.Random(0xCFF2143)
fuzz = {".notdef": ["endchar"]}
for i in range(512):
    raw = bytearray(rng.randrange(256) for _ in range(rng.randrange(0, 65)))
    for j in range(1, len(raw)):
        if raw[j - 1] == 12 and raw[j] == 23:
            raw[j] = 24
    fuzz[f"case{i}"] = bytes(raw)
font(fuzz).save(OUT / "malformed.otf")

seeded = {".notdef": ["endchar"], "random": ["random", 200, "mul", "random", 200, "mul", "rmoveto", 200, 200, "rlineto", "endchar"]}
font(seeded, private={"initialRandomSeed": 12345}).save(OUT / "seeded-random.otf")

# The Private DICT itself can blend its blue zones before the charstring
# is interpreted; these nonzero-peak regions contribute zero at default.
fb = font({".notdef": [], "triangle": triangle[:-1]}, cff2=True,
          regions=[{"wght": (0, 1, 1)}], private={"BlueValues": [[-20, 0], [0, 0], [680, 20], [700, 40]]})
fb.save(OUT / "private-blend-cff2.otf")

# Sixteen nested subroutine calls are accepted by cf2 (the Type 2 spec's
# smaller recommended limit is not the engine's actual bound).
programs = {".notdef": ["endchar"], "deep_ok": [-107, "callsubr", "endchar"],
            "deep_bad": [-91, "callsubr", "endchar"],
            "global_local": [-107, "callgsubr", "endchar"]}
fb = font(programs)
top = fb.font["CFF "].cff.topDictIndex[0]
subrs = SubrsIndex()
for i in range(17):
    code = [100, 100, "rmoveto", 200, 200, "rlineto", "return"] if i == 15 else [i + 1 - 107 if i < 15 else -91, "callsubr", "return"]
    subrs.append(T2CharString(program=code, private=top.Private, globalSubrs=top.GlobalSubrs))
top.Private.Subrs = subrs
top.GlobalSubrs.append(T2CharString(program=[-107, "callsubr", "return"], private=top.Private, globalSubrs=top.GlobalSubrs))
fb.save(OUT / "subroutines.otf")

# A tiny CID-keyed face covers FDSelect format 0 and per-FD blue zones.
from fontTools.cffLib import FDArrayIndex, FDSelect, FontDict, PrivateDict
fb = font({".notdef": ["endchar"], "cid00001": triangle,
           "cid00002": triangle, "cid00003": [0, 20, 680, 20, "hstem", *triangle]})
top = fb.font["CFF "].cff.topDictIndex[0]
top.ROS = ("Adobe", "Identity", 0)
top.CIDCount = 4
del top.Private
fds = FDArrayIndex()
fds.strings = None
fds.GlobalSubrs = top.GlobalSubrs
for name, blues in [("Lower", [-20, 0, 680, 700]), ("Upper", [-15, 0, 290, 310])]:
    fd = FontDict()
    fd.FontName = name
    fd.Private = PrivateDict()
    fd.Private.BlueValues = blues
    fds.append(fd)
select = FDSelect(format=0)
select.gidArray = [0, 0, 1, 0]
top.FDArray = fds
top.FDSelect = select
top.CharStrings.fdArray = fds
top.CharStrings.fdSelect = select
for i, name in enumerate(fb.font.getGlyphOrder()):
    top.CharStrings[name].private = fds[select.gidArray[i]].Private
fb.save(OUT / "cid.otf")

# With an sfnt fvar table, the default normalized vector preserves the
# contribution of an axis-independent (zero-peak) region.
fb = font({".notdef": [], "zero_peak": [100, 100, 200, 300, 2, "blend", "rmoveto", 100, 100, "rlineto"]},
          cff2=True, regions=[{"wght": (0, 0, 0)}])
fb.save(OUT / "zero-peak-cff2.otf")

# CFF2 multiple FDs, nonzero vsindex, and its larger operand stack.
# FreeType 2.14.3 rejects format 4; retain it as a negative fixture.
fb = font({".notdef": [], "fd_one": [100, 0, "rmoveto", 100, 300, "rlineto"],
           "vsindex_one": [1, "vsindex", 100, 0, 200, 30, 2, "blend", "rmoveto", 100, 300, "rlineto"],
           "large_stack": [1] * 513 + ["hlineto"],
           "overflow": [1] * 514 + ["hlineto"]},
          cff2=True, regions=[{"wght": (0, 1, 1)}, {"wght": (-1, -1, 0)}])
top = fb.font["CFF2"].cff.topDictIndex[0]
fd = FontDict()
fd.setCFF2(True)
fd.Private = PrivateDict()
fd.Private.BlueValues = [-20, 0, 280, 300]
top.FDArray.append(fd)
select = FDSelect(format=4)
select.gidArray = [0, 1, 0, 0, 0]
top.FDSelect = select
top.CharStrings.fdSelect = select
for i, name in enumerate(fb.font.getGlyphOrder()):
    top.CharStrings[name].private = top.FDArray[select.gidArray[i]].Private
from fontTools.varLib.builder import buildVarData
store = top.VarStore.otVarStore
store.VarData.append(buildVarData([1], [], optimize=False))
store.VarDataCount = len(store.VarData)
fb.save(OUT / "fdselect4-cff2.otf")

select.format = 3
fb.save(OUT / "multi-fd-cff2.otf")
