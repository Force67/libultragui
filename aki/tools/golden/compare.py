#!/usr/bin/env python3
"""Compare two golden dumps (ugui_golden.cc's format) and name the first
difference: which record, which frame, which buffer, which element and field.

    compare.py expected.bin actual.bin [--max N]

Exit status 0 when the files hold the same records, 1 otherwise.
"""
import argparse
import struct
import sys

VERTEX = struct.Struct("<2f2f3If2ffI")  # pos, uv, color, color2, radii, softness, half_size, border_width, border_color
VERTEX_FIELDS = ["pos.x", "pos.y", "uv.x", "uv.y", "color", "color2", "corner_radii",
                 "softness", "half_size.x", "half_size.y", "border_width", "border_color"]
CMD = struct.Struct("<4fQIIBf")
CMD_FIELDS = ["clip.x", "clip.y", "clip.w", "clip.h", "texture_id", "index_offset",
              "elem_count", "is_text", "blur"]
assert VERTEX.size == 48


class Reader:
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as f:
            self.data = f.read()
        self.pos = 0
        if len(self.data) < 8 or self.data[:4] != b"UGGD":
            raise ValueError(f"{path}: not a golden dump")
        (self.version,) = struct.unpack_from("<I", self.data, 4)
        if self.version != 1:
            raise ValueError(f"{path}: unsupported dump version {self.version}")
        self.pos = 8

    def take(self, fmt):
        s = struct.Struct("<" + fmt)
        return s.unpack(self.raw(s.size))

    def raw(self, n):
        if n > len(self.data) - self.pos:
            raise ValueError(f"{self.path}: truncated record at {self.pos} (need {n} bytes)")
        b = self.data[self.pos:self.pos + n]
        self.pos += n
        return b

    def records(self):
        while self.pos < len(self.data):
            tag = chr(self.data[self.pos])
            self.pos += 1
            if tag == "T":
                yield ("T",) + self.take("QIIBBQQ")
            elif tag == "U":
                yield ("U",) + self.take("QQQ")
            elif tag == "X":
                yield ("X",) + self.take("Q")
            elif tag == "A":
                yield ("A",) + self.take("IIIQ")
            elif tag == "F":
                # Preserve float bits: +0/-0 differ and equal NaN payloads
                # must compare equal, just as the vertex buffers do.
                head = self.raw(struct.calcsize("<I6fB"))
                bufs = []
                for size in (48, 4, 48, 4):
                    (n,) = self.take("I")
                    bufs.append((n, self.raw(n * size)))
                (ncmd,) = self.take("I")
                cmds = self.raw(ncmd * CMD.size)
                yield ("F", head, bufs, (ncmd, cmds))
            elif tag == "E":
                if self.pos != len(self.data):
                    raise ValueError(f"{self.path}: data after end marker at {self.pos}")
                yield ("E",)
                return
            else:
                raise ValueError(f"bad tag {tag!r} at {self.pos - 1}")
        raise ValueError(f"{self.path}: missing end marker")


def describe(rec):
    if rec is None:
        return "end of file"
    if rec[0] == "F":
        return f"frame {struct.unpack_from('<I', rec[1])[0]}"
    return repr(rec)


def diff_frame(a, b, out, limit):
    frame = struct.unpack_from("<I", a[1])[0]
    if a[1] != b[1]:
        out.append(f"frame {frame}: header {a[1].hex()} != {b[1].hex()}")
    names = ["quad vertices", "quad indices", "text vertices", "text indices"]
    for name, (na, ba), (nb, bb) in zip(names, a[2], b[2]):
        if na != nb:
            out.append(f"frame {frame}: {name} count {na} != {nb}")
        if ba == bb:
            continue
        if "vertices" in name:
            for i in range(min(na, nb)):
                va = VERTEX.unpack_from(ba, i * 48)
                vb = VERTEX.unpack_from(bb, i * 48)
                if ba[i * 48:(i + 1) * 48] != bb[i * 48:(i + 1) * 48]:
                    fields = [f"{f}: {x!r} != {y!r}" for f, x, y in zip(VERTEX_FIELDS, va, vb) if x != y]
                    if not fields:
                        fields = ["float bit patterns differ"]
                    out.append(f"frame {frame}: {name}[{i}] " + ", ".join(fields))
                    if len(out) >= limit:
                        return
        else:
            ia = struct.unpack(f"<{na}I", ba)
            ib = struct.unpack(f"<{nb}I", bb)
            for i, (x, y) in enumerate(zip(ia, ib)):
                if x != y:
                    out.append(f"frame {frame}: {name}[{i}] {x} != {y}")
                    if len(out) >= limit:
                        return
    (ca, da), (cb, db) = a[3], b[3]
    if ca != cb:
        out.append(f"frame {frame}: command count {ca} != {cb}")
    for i in range(min(ca, cb)):
        x = CMD.unpack_from(da, i * CMD.size)
        y = CMD.unpack_from(db, i * CMD.size)
        if da[i * CMD.size:(i + 1) * CMD.size] != db[i * CMD.size:(i + 1) * CMD.size]:
            fields = [f"{f}: {p!r} != {q!r}" for f, p, q in zip(CMD_FIELDS, x, y) if p != q]
            if not fields:
                fields = ["float bit patterns differ"]
            out.append(f"frame {frame}: command[{i}] " + ", ".join(fields))
            if len(out) >= limit:
                return


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("expected")
    ap.add_argument("actual")
    ap.add_argument("--max", type=int, default=20, help="differences to list")
    args = ap.parse_args()
    if args.max < 1:
        ap.error("--max must be positive")
    ea = Reader(args.expected).records()
    eb = Reader(args.actual).records()
    frames = 0
    index = 0
    while True:
        a = next(ea, None)
        b = next(eb, None)
        if a is None and b is None:
            print(f"same: {frames} frames")
            return 0
        if a is None or b is None or a[0] != b[0]:
            print(f"record {index}: expected {describe(a)}, got {describe(b)}")
            return 1
        if a[0] == "F":
            frames += 1
            if a != b:
                out = []
                diff_frame(a, b, out, args.max)
                print(f"record {index}: {describe(a)} differs")
                for line in out:
                    print("  " + line)
                return 1
        elif a != b:
            print(f"record {index}: expected {a!r}, got {b!r}")
            return 1
        index += 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, struct.error) as e:
        sys.exit(str(e))
