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
    "late_vsindex": [100, 50, 1, "blend", 0, "vsindex"],
}
fb = font(programs2, cff2=True, regions=[{"wght": (0, 1, 1)}], private={"BlueValues": [-20, 0, 680, 700]})
fb.save(OUT / "default-cff2.otf")
print(OUT)
