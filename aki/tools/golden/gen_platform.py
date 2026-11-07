#!/usr/bin/env python3
"""Regenerate golden_platform.cc from ugui/platform/platform_host.cc.

The golden runner needs the host platform with a clock it controls: animation,
transitions, tooltips and Lua timers all read Platform::time(), and a wall
clock would make every run a different picture.
"""
import pathlib

here = pathlib.Path(__file__).resolve().parent
src = (here / "../../../ugui/platform/platform_host.cc").read_text()
old = "f64 Platform::time() const {\n  return SecondsSince(impl_->start);\n}"
assert old in src, "platform_host.cc changed; update gen_platform.py"
src = src.replace(old, "f64 Platform::time() const { return ugui_golden_time; }")
src = src.replace("namespace ugui {\n\nnamespace {",
                  "double ugui_golden_time = 0.0;\n\nnamespace ugui {\n\nnamespace {", 1)
hdr = ("// Generated from ugui/platform/platform_host.cc by aki/tools/golden/gen_platform.py:\n"
       "// the host platform, with Platform::time() reading ugui_golden_time instead\n"
       "// of the steady clock, so a golden run is the same picture on every machine.\n")
(here / "golden_platform.cc").write_text(hdr + src)
