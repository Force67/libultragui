#!/usr/bin/env python3
# Turns gen_reference's output (see gen_reference.cc) into
# tests/support/lottie_reference.aki:
#   /tmp/gen_lottie_reference > /tmp/lottie_ref.txt
#   python3 -I tools/gen_tables.py /tmp/lottie_ref.txt > tests/support/lottie_reference.aki
import sys

loads = []
rows = []
counts = []
for line in open(sys.argv[1]):
    line = line.strip()
    if not line or line.startswith('//') or line.startswith('game '):
        continue
    t = line.split()
    if t[0] == 'LOAD':
        loads.append(t[1:])
    elif t[0] in ('SEEK', 'PLAY', 'ONCE'):
        rows.append((t[0],) + tuple(t[1:]))
    elif t[0] == 'END':
        playing = int(t[4].split('=')[1])
        rows.append(('END', t[1], t[2], t[3], '0', str(playing), '0x00000000', '0x0000000000000000'))
        rows.append(('ONCE_END',) + tuple(t[6:]))
    elif t[0] == 'COUNTS':
        counts.append((t[1], t[2], t[3]) + tuple(int(x.split('=')[1]) for x in t[4:]))
    elif t[0] == 'UNLOADED':
        d = int(t[4].split('=')[1])
        live = int(t[5].split('=')[1])
        counts[-1] = counts[-1] + (d, live)
    else:
        raise Exception(line)

out = '''// Recorded from the C++ LottieAnimation by tools/gen_reference.cc (aarch64
// Linux, rlottie from the reference build). Regenerate with tools/gen_tables.py;
// do not edit by hand.

module support::lottie_reference;

// LottieAnimation::Load of examples/lottie_demo/anims/<name>.json at w x h:
// total_frames, frame_rate and duration as f64 bits, FNV-1a of the first
// frame's RGBA.
pub struct LoadRef : Copy {
  pub let name: str
  pub let w: u32
  pub let h: u32
  pub let total_frames: u32
  pub let frame_rate_bits: u64
  pub let duration_bits: u64
  pub let hash: u64
}

// One check after a Seek ("SEEK"), a Play-ing Update ("PLAY"), a non-looping
// Update ("ONCE") or the end of the non-looping run ("END": `frame` is
// is_playing, "ONCE_END": the state at the end): total_frames, current_frame,
// progress as f32 bits, FNV-1a of the texture's RGBA.
pub struct StepRef : Copy {
  pub let kind: str
  pub let name: str
  pub let w: u32
  pub let h: u32
  pub let total_frames: u32
  pub let frame: u32
  pub let progress_bits: u32
  pub let hash: u64
}

// Texture backend calls per animation: creates, updates, destroys before
// unload, destroys after unload, textures still live after unload.
pub struct CountsRef : Copy {
  pub let name: str
  pub let w: u32
  pub let h: u32
  pub let creates: u32
  pub let updates: u32
  pub let destroys_before_unload: u32
  pub let destroys: u32
  pub let live: u32
}

'''
out += 'pub static let kLoads: [LoadRef; %d] = [\n' % len(loads)
for l in loads:
    out += '  LoadRef { name: "%s", w: %s, h: %s, total_frames: %s, frame_rate_bits: %s, duration_bits: %s, hash: %s },\n' % tuple(l)
out += '];\n\n'
out += 'pub static let kSteps: [StepRef; %d] = [\n' % len(rows)
for r in rows:
    out += '  StepRef { kind: "%s", name: "%s", w: %s, h: %s, total_frames: %s, frame: %s, progress_bits: %s, hash: %s },\n' % r
out += '];\n\n'
out += 'pub static let kCounts: [CountsRef; %d] = [\n' % len(counts)
for c in counts:
    out += '  CountsRef { name: "%s", w: %s, h: %s, creates: %d, updates: %d, destroys_before_unload: %d, destroys: %d, live: %d },\n' % c
out += '];\n'
sys.stdout.write(out)
