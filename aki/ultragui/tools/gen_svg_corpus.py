#!/usr/bin/env python3
"""Writes the random scenes of tests/data/svg/ (scene_N.svg): paths mixing
every command, transforms, solid/gradient paints, strokes, fill rules. The
files are checked in; this documents how they were made.

    python3 aki/ultragui/tools/gen_svg_corpus.py aki/ultragui/tests/data/svg
"""
import random
import sys

def num(r, lo, hi):
    return round(r.uniform(lo, hi), r.choice([0, 1, 2, 3]))

def pt(r, w, h):
    return f"{num(r, -0.1 * w, 1.1 * w)},{num(r, -0.1 * h, 1.1 * h)}"

def path_data(r, w, h):
    d = [f"M{pt(r, w, h)}"]
    for _ in range(r.randint(2, 7)):
        k = r.choice("LHVCSQTAZlhvcsqta")
        if k in "Zz":
            d.append("Z")
            d.append(f"M{pt(r, w, h)}")
        elif k in "Ll":
            d.append(k + pt(r, w, h) if k == "L" else k + f"{num(r, -30, 30)},{num(r, -30, 30)}")
        elif k in "Hh":
            d.append(k + str(num(r, -30 if k == "h" else 0, 30 if k == "h" else w)))
        elif k in "Vv":
            d.append(k + str(num(r, -30 if k == "v" else 0, 30 if k == "v" else h)))
        elif k in "Cc":
            if k == "C":
                d.append("C" + " ".join(pt(r, w, h) for _ in range(3)))
            else:
                d.append("c" + " ".join(f"{num(r, -30, 30)},{num(r, -30, 30)}" for _ in range(3)))
        elif k in "Ss":
            if k == "S":
                d.append("S" + " ".join(pt(r, w, h) for _ in range(2)))
            else:
                d.append("s" + " ".join(f"{num(r, -30, 30)},{num(r, -30, 30)}" for _ in range(2)))
        elif k in "Qq":
            if k == "Q":
                d.append("Q" + " ".join(pt(r, w, h) for _ in range(2)))
            else:
                d.append("q" + " ".join(f"{num(r, -30, 30)},{num(r, -30, 30)}" for _ in range(2)))
        elif k in "Tt":
            d.append("T" + pt(r, w, h) if k == "T" else "t" + f"{num(r, -30, 30)},{num(r, -30, 30)}")
        else:
            rx, ry = num(r, 1, 40), num(r, 1, 40)
            rot = num(r, -90, 90)
            la, sw = r.choice([0, 1]), r.choice([0, 1])
            if k == "A":
                d.append(f"A{rx},{ry} {rot} {la},{sw} {pt(r, w, h)}")
            else:
                d.append(f"a{rx},{ry} {rot} {la} {sw} {num(r, -40, 40)},{num(r, -40, 40)}")
    return " ".join(d)

def transform(r):
    parts = []
    for _ in range(r.randint(0, 3)):
        k = r.choice(["translate", "scale", "rotate", "skewX", "skewY", "matrix", "rotatec"])
        if k == "translate":
            parts.append(f"translate({num(r, -20, 20)} {num(r, -20, 20)})")
        elif k == "scale":
            parts.append(f"scale({num(r, 0.3, 2)}{'' if r.random() < .5 else ' ' + str(num(r, 0.3, 2))})")
        elif k == "rotate":
            parts.append(f"rotate({num(r, -180, 180)})")
        elif k == "rotatec":
            parts.append(f"rotate({num(r, -180, 180)} {num(r, 0, 100)} {num(r, 0, 100)})")
        elif k in ("skewX", "skewY"):
            parts.append(f"{k}({num(r, -40, 40)})")
        else:
            parts.append("matrix(" + " ".join(str(num(r, -1.5, 1.5)) for _ in range(4)) +
                         f" {num(r, -20, 20)} {num(r, -20, 20)})")
    return " ".join(parts)

def color(r):
    c = r.choice(["hex6", "hex3", "hex8", "named", "rgb", "rgba"])
    if c == "hex6":
        return "#%06x" % r.randrange(1 << 24)
    if c == "hex3":
        return "#%03x" % r.randrange(1 << 12)
    if c == "hex8":
        return "#%08x" % r.randrange(1 << 32)
    if c == "named":
        return r.choice(["red", "navy", "teal", "gold", "coral", "orchid", "peru", "salmon", "skyblue", "olive"])
    if c == "rgb":
        return f"rgb({r.randrange(256)},{r.randrange(256)},{r.randrange(256)})"
    return f"rgba({r.randrange(256)},{r.randrange(256)},{r.randrange(256)},{round(r.random(), 2)})"

def gradient(r, gid, w, h):
    stops = sorted(round(r.random(), 2) for _ in range(r.randint(2, 4)))
    if r.random() < 0.3 and len(stops) > 2:
        stops[1] = stops[2]
    stop_xml = "".join(
        f'<stop offset="{s}" stop-color="{color(r)}"{"" if r.random() < .6 else f" stop-opacity=\"{round(r.random(), 2)}\""}/>'
        for s in stops)
    spread = r.choice(["pad", "reflect", "repeat"])
    units = r.choice(["", ' gradientUnits="userSpaceOnUse"'])
    gt = r.choice(["", f' gradientTransform="{transform(r)}"'])
    if r.random() < 0.5:
        if units:
            geo = f'x1="{num(r, 0, w)}" y1="{num(r, 0, h)}" x2="{num(r, 0, w)}" y2="{num(r, 0, h)}"'
        else:
            geo = f'x1="{num(r, 0, 1)}" y1="{num(r, 0, 1)}" x2="{num(r, 0, 1)}" y2="{num(r, 0, 1)}"'
        return f'<linearGradient id="{gid}"{units}{gt} spreadMethod="{spread}" {geo}>{stop_xml}</linearGradient>'
    if units:
        geo = f'cx="{num(r, 0, w)}" cy="{num(r, 0, h)}" r="{num(r, 5, 80)}"'
    else:
        geo = f'cx="{num(r, 0, 1)}" cy="{num(r, 0, 1)}" r="{num(r, 0.1, 0.9)}"'
    return f'<radialGradient id="{gid}"{units}{gt} spreadMethod="{spread}" {geo}>{stop_xml}</radialGradient>'

def scene(seed, w=160, h=120):
    r = random.Random(seed)
    n_grad = r.randint(2, 4)
    out = [f'<svg width="{w}" height="{h}" xmlns="http://www.w3.org/2000/svg">', "<defs>"]
    out += [gradient(r, f"g{i}", w, h) for i in range(n_grad)]
    out.append("</defs>")
    for _ in range(r.randint(18, 28)):
        attrs = []
        paint = r.random()
        if paint < 0.3:
            attrs.append(f'fill="url(#g{r.randrange(n_grad)})"')
        elif paint < 0.85:
            attrs.append(f'fill="{color(r)}"')
        else:
            attrs.append('fill="none"')
        if r.random() < 0.5:
            s = f"url(#g{r.randrange(n_grad)})" if r.random() < 0.2 else color(r)
            attrs.append(f'stroke="{s}" stroke-width="{num(r, 0.2, 7)}"')
        if r.random() < 0.3:
            attrs.append(f'opacity="{round(r.random(), 2)}"')
        if r.random() < 0.3:
            attrs.append(f'fill-opacity="{round(r.random(), 2)}"')
        if r.random() < 0.3:
            attrs.append(f'stroke-opacity="{round(r.random(), 2)}"')
        if r.random() < 0.4:
            attrs.append('fill-rule="evenodd"')
        t = transform(r)
        if t:
            attrs.append(f'transform="{t}"')
        shape = r.choice(["path", "path", "path", "rect", "circle", "ellipse", "polygon", "polyline", "line"])
        if shape == "path":
            out.append(f'<path d="{path_data(r, w, h)}" {" ".join(attrs)}/>')
        elif shape == "rect":
            rr = f' rx="{num(r, 0, 20)}"' if r.random() < .5 else ""
            out.append(f'<rect x="{num(r, 0, w)}" y="{num(r, 0, h)}" width="{num(r, 1, 80)}" height="{num(r, 1, 60)}"{rr} {" ".join(attrs)}/>')
        elif shape == "circle":
            out.append(f'<circle cx="{num(r, 0, w)}" cy="{num(r, 0, h)}" r="{num(r, 1, 40)}" {" ".join(attrs)}/>')
        elif shape == "ellipse":
            out.append(f'<ellipse cx="{num(r, 0, w)}" cy="{num(r, 0, h)}" rx="{num(r, 1, 50)}" ry="{num(r, 1, 30)}" {" ".join(attrs)}/>')
        elif shape == "line":
            out.append(f'<line x1="{num(r, 0, w)}" y1="{num(r, 0, h)}" x2="{num(r, 0, w)}" y2="{num(r, 0, h)}" {" ".join(attrs)}/>')
        else:
            pts = " ".join(pt(r, w, h) for _ in range(r.randint(3, 8)))
            out.append(f'<{shape} points="{pts}" {" ".join(attrs)}/>')
    out.append("</svg>")
    return "\n".join(out) + "\n"

if __name__ == "__main__":
    d = sys.argv[1]
    for i, seed in enumerate([1, 2, 3, 4, 5, 6, 7, 8]):
        open(f"{d}/scene_{i + 1}.svg", "w").write(scene(seed))
