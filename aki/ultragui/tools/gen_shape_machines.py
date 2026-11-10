#!/usr/bin/env python3
"""Extracts the syllable machines and vowel constraints of HarfBuzz 13.2.1
into src/shape/machines.aki.

  python3 tools/gen_shape_machines.py <harfbuzz-13.2.1>/src

HarfBuzz's syllable finders (hb-ot-shaper-{indic,khmer,myanmar,use}-machine.hh)
are Ragel-generated scanners: tables plus an exec loop whose actions set
ts/te/act, rewind p and mark syllables. This takes the tables as they are and
compiles each action into a list of operations that ultragui::shape::syllabic's
generic exec loop interprets:

  1 TE_P1      te = p + 1
  2 TE_P_DEC   te = p; p--
  3 P_TE       p = te - 1
  4 FOUND t    found_syllable (t)
  5 BROKEN     scratch_flags |= HAS_BROKEN_SYLLABLE
  6 ACT n      act = n
  7 ACT_SWITCH run the operations of the current act

The vowel constraints (hb-ot-shaper-vowel-constraints.cc, generated code too)
become (script, first, second) rows.
"""

import os
import re
import sys

OPS = {"TE_P1": 1, "TE_P_DEC": 2, "P_TE": 3, "FOUND": 4, "BROKEN": 5, "ACT": 6, "ACT_SWITCH": 7}


def array(text, name):
    m = re.search(r"static const (?:unsigned char|char|short|unsigned short|int) " + name + r"\[\] = \{(.*?)\};", text, re.S)
    assert m, name
    return [int(x.strip().rstrip("u")) for x in m.group(1).replace("\n", " ").split(",") if x.strip()]


def scalar(text, name):
    m = re.search(r"static const int " + name + r" = (-?\d+);", text)
    assert m, name
    return int(m.group(1))


def enum_values(text, enum_name):
    m = re.search(r"enum " + enum_name + r" \{(.*?)\};", text, re.S)
    assert m, enum_name
    vals = {}
    i = 0
    for item in m.group(1).split(","):
        item = re.sub(r"/\*.*?\*/", "", item).strip()
        if not item:
            continue
        name = item.split("=")[0].strip()
        if "=" in item:
            i = int(item.split("=")[1].strip(), 0)
        vals[name] = i
        i += 1
    return vals


TOKEN = re.compile(r"te = p\+1|te = p;p--|p = \(\(te\)\)-1|found_syllable \((\w+)\)|HAS_BROKEN_SYLLABLE|act = (\d+)")


def ops_of(body, types):
    ops = []
    for m in TOKEN.finditer(body):
        s = m.group(0)
        if s == "te = p+1":
            ops.append((OPS["TE_P1"], 0))
        elif s == "te = p;p--":
            ops.append((OPS["TE_P_DEC"], 0))
        elif s.startswith("p = "):
            ops.append((OPS["P_TE"], 0))
        elif s.startswith("found_syllable"):
            ops.append((OPS["FOUND"], types[m.group(1)]))
        elif s == "HAS_BROKEN_SYLLABLE":
            ops.append((OPS["BROKEN"], 0))
        else:
            ops.append((OPS["ACT"], int(m.group(2))))
    return ops


def machine(src, name, enum_name):
    text = open(os.path.join(src, f"hb-ot-shaper-{name}-machine.hh")).read()
    pfx = f"_{name}_syllable_machine_"
    types = enum_values(text, enum_name)
    d = {}
    for a in ["trans_keys", "key_spans", "index_offsets", "indicies", "trans_targs", "trans_actions", "to_state_actions",
              "from_state_actions", "eof_trans"]:
        d[a] = array(text, pfx + a)
    d["start"] = scalar(text, f"{name}_syllable_machine_start")
    # from/to state actions only ever set ts.
    body = text[text.index("switch ( " + pfx + "from_state_actions[cs] ) {"):]
    from_case = body[:body.index("}")]
    assert "ts = p;" in from_case
    body = text[text.index("switch ( " + pfx + "to_state_actions[cs] ) {"):]
    to_case = body[:body.index("}")]
    assert "ts = 0;" in to_case
    # The action switch.
    start = text.index("switch ( " + pfx + "trans_actions[_trans] ) {")
    end = text.index("_again:", start)
    sw = text[start:end]
    sw = re.sub(r"#line[^\n]*\n", "", sw)
    cases = re.split(r"\n\tcase (\d+):", sw)
    actions = {}
    act_switch = {}
    in_act = False
    for i in range(1, len(cases), 2):
        num = int(cases[i])
        body = cases[i + 1]
        if in_act:
            # A case of the inner switch (act); the last one closes it.
            act_switch[num] = ops_of(body, types)
            if re.search(r"break;\s*\}\s*\}\s*break;", body):
                in_act = False
            continue
        if "switch( act )" in body:
            actions[num] = [(OPS["ACT_SWITCH"], 0)]
            in_act = True
        else:
            actions[num] = ops_of(body, types)
    # Flatten: per action number an offset into ops (pairs), 0-terminated.
    n_actions = max(max(d["trans_actions"]), max(actions.keys())) + 1
    ops = [0, 0]  # offset 0: no operations
    action_start = [0] * n_actions
    for num in sorted(actions):
        action_start[num] = len(ops) // 2
        for op in actions[num]:
            ops += list(op)
        ops += [0, 0]
    n_acts = max(act_switch.keys()) + 1 if act_switch else 1
    act_start = [0] * n_acts
    for num in sorted(act_switch):
        act_start[num] = len(ops) // 2
        for op in act_switch[num]:
            ops += list(op)
        ops += [0, 0]
    d["ops"] = ops
    d["action_start"] = action_start
    d["act_start"] = act_start
    d["types"] = types
    return d


def script_tags(src):
    text = open(os.path.join(src, "hb-script-list.h")).read()
    tags = {}
    for m in re.finditer(r"HB_SCRIPT_(\w+)\s*=\s*HB_TAG \('(.)','(.)','(.)','(.)'\)", text):
        tags[m.group(1)] = m.group(2) + m.group(3) + m.group(4) + m.group(5)
    return tags


def vowel_constraints(src):
    text = open(os.path.join(src, "hb-ot-shaper-vowel-constraints.cc")).read()
    tags = script_tags(src)
    rows = []
    blocks = re.split(r"\n    case HB_SCRIPT_(\w+):", text)
    for i in range(1, len(blocks), 2):
        script = tags[blocks[i]]
        body = blocks[i + 1]
        body = body[:body.index("\n      break;")] if "\n      break;" in body else body
        # Each "case 0x....u: [case ...:] <rule>" at the first-character level (8 tabs/1 tab + 2 spaces).
        for m in re.finditer(r"((?:\t  case 0x[0-9A-F]+u:\s*)+)(.*?)\n\t    break;", body, re.S):
            firsts = [int(x, 16) for x in re.findall(r"0x([0-9A-F]+)u", m.group(1))]
            rule = m.group(2)
            if "cur (2)" in rule:
                seconds = [int(x, 16) for x in re.findall(r"0x([0-9A-F]+)u", rule)]
                assert len(seconds) == 2
                for f in firsts:
                    rows.append((script, f, seconds[0], seconds[1]))
                continue
            seconds = [int(x, 16) for x in re.findall(r"0x([0-9A-F]+)u", rule)]
            for f in firsts:
                for s in seconds:
                    rows.append((script, f, s, 0))
    return rows


def arr(name, typ, values, per_line=16):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append("  " + ", ".join(str(v) for v in values[i:i + per_line]) + ",")
    return f"pub static let {name}: [{typ}; {len(values)}] = [\n" + "\n".join(lines) + "\n];\n"


def main():
    src = sys.argv[1]
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "src", "shape", "machines.aki")
    w = ["// Generated by tools/gen_shape_machines.py from HarfBuzz 13.2.1's Ragel\n"
         "// syllable machines (hb-ot-shaper-*-machine.hh) and vowel constraints\n"
         "// (hb-ot-shaper-vowel-constraints.cc). Do not edit. See the tool for the\n"
         "// operation encoding ultragui::shape::syllabic interprets.\n\n"
         "module ultragui::shape::machines;\n\n"]
    for name, enum_name in [("indic", "indic_syllable_type_t"), ("khmer", "khmer_syllable_type_t"),
                            ("myanmar", "myanmar_syllable_type_t"), ("use", "use_syllable_type_t")]:
        d = machine(src, name, enum_name)
        up = name.capitalize()
        w.append(f"// {name}: syllable types " + ", ".join(f"{k} = {v}" for k, v in d["types"].items()) + "\n")
        w.append(f"pub comptime let k{up}Start: i32 = {d['start']};\n")
        w.append(arr(f"k{up}TransKeys", "u8", d["trans_keys"]))
        w.append(arr(f"k{up}KeySpans", "u8", d["key_spans"]))
        w.append(arr(f"k{up}IndexOffsets", "u16", d["index_offsets"]))
        w.append(arr(f"k{up}Indicies", "u16", d["indicies"]))
        w.append(arr(f"k{up}TransTargs", "u16", d["trans_targs"]))
        w.append(arr(f"k{up}TransActions", "u8", d["trans_actions"]))
        w.append(arr(f"k{up}ToStateActions", "u8", d["to_state_actions"]))
        w.append(arr(f"k{up}FromStateActions", "u8", d["from_state_actions"]))
        w.append(arr(f"k{up}EofTrans", "u16", d["eof_trans"]))
        w.append(arr(f"k{up}Ops", "u8", d["ops"]))
        w.append(arr(f"k{up}ActionStart", "u16", d["action_start"]))
        w.append(arr(f"k{up}ActStart", "u16", d["act_start"]))
        w.append("\n")
    rows = vowel_constraints(src)

    def tag(s):
        return (ord(s[0]) << 24) | (ord(s[1]) << 16) | (ord(s[2]) << 8) | ord(s[3])
    flat = []
    for r in rows:
        flat += [tag(r[0]), r[1], r[2], r[3]]
    w.append("// Vowel constraints: script, first, second, third (0: a pair rule).\n")
    w.append(arr("kVowelConstraints", "u32", flat, 8))
    open(out, "w").write("".join(w))
    print(f"{len(rows)} vowel constraint rows")


main()
