#!/usr/bin/env python3
# Turns gen_reference's output (see gen_reference.cc) into
# tests/support/video_reference.aki. The scenario output is the same with and
# without an AudioBackend (the video does not depend on the audio clock), so
# the table is the "none" run; the one thing that differs, set_volume and
# set_muted needing the audio bridge, is the VOL lines.
#   python3 -I tools/gen_tables.py /tmp/video_ref.txt > tests/support/video_reference.aki
import sys

load = None
load2 = None
convert = None
states = []
states_audio = []
vol = {}
unload = {}
for line in open(sys.argv[1]):
    t = line.split()
    if not t or t[0].startswith('//'):
        continue
    if t[0] == 'LOAD' and t[1] == 'none':
        load = t[2:]
    elif t[0] == 'LOAD2' and t[1] == 'none':
        load2 = t[2:]
    elif t[0] == 'CONVERT' and t[1] == 'none':
        convert = t[2:]
    elif t[0] == 'STATE':
        (states if t[2] == 'none' else states_audio).append(t[1:2] + t[3:])
    elif t[0] == 'VOL':
        vol[t[1]] = t[2:]
    elif t[0] == 'UNLOAD':
        unload[t[1]] = t[2:]
assert states == states_audio, 'audio changes the video'

o = '''// Recorded from the C++ VideoPlayer decoding examples/demo.mpg through a
// recording RHI (tools/gen_reference.cc, aarch64 Linux). Regenerate with
// tools/gen_tables.py; do not edit by hand.

module support::video_reference;

// After each step of the scripted run: pl_mpeg's clock (position as f64
// bits), the player's flags, the RHI call counters, and FNV-1a of the three
// plane textures (all three together, then each).
pub struct StateRef : Copy {
  pub let scenario: str
  pub let step: i32
  pub let position_bits: u64
  pub let finished: bool
  pub let playing: bool
  pub let needs_convert: bool
  pub let updates: u32
  pub let creates: u32
  pub let converts: u32
  pub let frame_hash: u64
  pub let y_hash: u64
  pub let cb_hash: u64
  pub let cr_hash: u64
}

'''
w, h, dur, fr, tex, y, cb, cr, yw, yh = load
cbw = str(int(yw) // 2)  # gen_reference prints the Cb width's height only
cbh, volb, speedb, muted = load2
o += 'pub comptime let kWidth: u32 = %s;\npub comptime let kHeight: u32 = %s;\n' % (w, h)
o += 'pub comptime let kDurationBits: u64 = %s;\npub comptime let kFrameRateBits: u64 = %s;\n' % (dur, fr)
o += '// Handles the recording RHI hands out in order: render target 0, then Y, Cb, Cr.\n'
o += 'pub comptime let kTextureHandle: u32 = %s;\npub comptime let kYHandle: u32 = %s;\npub comptime let kCbHandle: u32 = %s;\npub comptime let kCrHandle: u32 = %s;\n' % (tex, y, cb, cr)
o += 'pub comptime let kPlaneW: u32 = %s;\npub comptime let kPlaneH: u32 = %s;\npub comptime let kChromaW: u32 = %s;\npub comptime let kChromaH: u32 = %s;\n' % (yw, yh, cbw, cbh)
o += '// ConvertFrame before any Update: target, y, cb, cr.\n'
o += 'pub static let kFirstConvert: [u32; 4] = [%s];\n' % ', '.join(convert)
o += '// set_volume(0.5); set_muted(true) then volume() bits, muted(): with and without an audio bridge.\n'
o += 'pub comptime let kVolumeWithBridgeBits: u32 = %s;\npub comptime let kMutedWithBridge: bool = %s;\n' % (vol['audio'][0], 'true' if vol['audio'][1] == '1' else 'false')
o += 'pub comptime let kVolumeWithoutBridgeBits: u32 = %s;\npub comptime let kMutedWithoutBridge: bool = %s;\n' % (vol['none'][0], 'true' if vol['none'][1] == '1' else 'false')
d, tc, td, cv, live, loaded = unload['none']
o += '// After unload: destroy_texture calls, render targets created and destroyed, converts, textures live.\n'
o += 'pub comptime let kUnloadDestroys: u32 = %s;\npub comptime let kTargetCreates: u32 = %s;\npub comptime let kTargetDestroys: u32 = %s;\npub comptime let kTotalConverts: u32 = %s;\npub comptime let kLiveAfterUnload: usize = %s;\n\n' % (d, tc, td, cv, live)
o += 'pub static let kStates: [StateRef; %d] = [\n' % len(states)
for s in states:
    sc, step, pos, fin, play, nc, up, cr_, cv_, fh, yh, cbh_, crh = s
    o += '  StateRef { scenario: "%s", step: %s, position_bits: %s, finished: %s, playing: %s, needs_convert: %s, updates: %s, creates: %s, converts: %s, frame_hash: %s, y_hash: %s, cb_hash: %s, cr_hash: %s },\n' % (
        sc, step, pos, 'true' if fin == '1' else 'false', 'true' if play == '1' else 'false', 'true' if nc == '1' else 'false', up, cr_, cv_, fh, yh, cbh_, crh)
o += '];\n'
sys.stdout.write(o)
