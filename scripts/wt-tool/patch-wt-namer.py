#!/usr/bin/env python3
"""
patch-wt-namer.py

Attaches proper wavetable names to factory and 3rd party patches whose
oscillators currently display "(Patch Wavetable)", by identifying the
embedded table against the shipped wavetable library.

Run from the Surge XT repository root:

    python3 scripts/wt-tool/patch-wt-namer.py             # report only
    python3 scripts/wt-tool/patch-wt-namer.py --apply     # rewrite the patches

--- Why patches say "(Patch Wavetable)" ---

Every oscillator of type Wavetable or Window writes its whole table into
the .fxp. The name is streamed separately, as a wavetable_display_name
attribute inside the <extraoscdata> element. A patch shows the fallback
label when that attribute is missing -- patches older than the attribute,
streaming revisions 7 to 12, have no <extraoscdata> element at all -- or
when it literally holds the placeholder string, which happens when a
patch was re-saved after being loaded with the fallback in place.

Nothing about this is revision-gated. SurgePatch::load_xml reads
<extraoscdata> purely on the element being present, and load_patch reads
the XML before the wavetable chunks, so a name present in the file wins
over the fallback. Adding names to old patches therefore needs no
revision bump.

--- What this script does ---

Case A -- clean rename:
The embedded table is identical to a shipped wavetable, within one LSB of
the int16 quantization save_patch applies to embedded tables. Only the
name is written. Nothing else about the patch changes.

Case B -- near rename (see --near-lsb):
As case A but the table differs by a few LSB, below audibility and with
identical dimensions. Only the name is written.

Case B2 -- all but a few samples identical:
The tables agree bit for bit except at a handful of samples. This is a
rendering difference at a waveform discontinuity, not a different
wavetable: Organ 3 against Saw Asymmetric differs by 15879 steps at
exactly one sample out of 16384 and by nothing anywhere else, because the
two renderings put the sawtooth's reset in a slightly different place.
Judging by the largest difference alone would reject these; counting how
many samples disagree does not. Name only, as with case A.

Case C -- short frame count:
The embedded table is the leading frames of a shipped wavetable, rather
than a reslice of it.

Nearly all of these come from one cause. Before commit 9a6301ac3 ("Fix
factory wavetables", issue #8013) a number of Waldorf .wt files declared
60 frames in their header while physically holding 63 or 64, so Surge
only ever read 60 frames and any patch saved against them embedded a 60
frame table. The sample data itself never changed across that fix.

One case is not that: two slots hold the first 4 frames of the 6 frame
Generated/Saw ATC.wav. The handling is the same either way, since what
matters is only that the patch holds fewer frames than the file does.

For these the script does three things together, because doing any one of
them alone changes how the patch sounds:

  1. writes the name;
  2. re-embeds the wavetable from the current file, so the patch carries
     the full table rather than the stale short one;
  3. rescales Morph, and every modulation depth routed to Morph, so the
     oscillator still lands on the frame it landed on before.

Step 3 is needed because Morph is normalized across the frame count.
WavetableOscillator.cpp does

    nointerp = !oscdata->p[wt_morph].extend_range;
    shape *= ((float)oscdata->wt.n_tables - 1.f + nointerp) * 0.99999f;

so with span(n) = n - 1 + nointerp the correction is a plain ratio,

    k = span(old_frames) / span(new_frames)

applied to the Morph value and to each routing depth alike. Depth is in
the same normalized Morph units as the base value, so scaling both
reproduces the whole frame trajectory, not merely the resting position.

Two notes on the limits of that correction:

  - Clipping at the floor is harmless. The old patch clamps when
    morph + mod < 0 and the corrected one when k * (morph + mod) < 0,
    which is the same condition, and both pin to frame 0.
  - Clipping at the ceiling is not. The old patch clamps above 1.0 and
    pins to the last of the 60 frames; the corrected patch only clamps
    above 1/k, so within that band it travels on into the restored frames
    instead of holding, which is audibly different from what ships today.

    A handful of slots reach that far. They were auditioned, starting with
    the most exposed of them -- Anomaly, whose modulation asks for 1.4902
    -- and the character survives, so they are corrected along with the
    rest. Pass --hold-ceiling-clippers to leave them alone and have them
    reported instead.

Case E -- curated ear match:
A small hand-kept table, EAR_MATCHES below, for slots that are audibly a
shipped wavetable but a different rendering of it, so no honest numeric
tolerance can match them. Each entry says whether the table may also be
re-embedded, which is a per-patch listening decision rather than
something the numbers can settle: re-embedding makes the name truthful
and removes the reattach surprise, at the cost of changing how the patch
sounds today. An entry that declines it gets the name only.
--skip-ear-matches leaves the whole set alone.

Case D -- retired name:
The oscillator already names a wavetable that no longer ships, because
that file was removed as a byte-for-byte duplicate of another. The name
is rewritten to the surviving file's. See RETIRED_NAMES below. The
embedded table is checked against the replacement first, so a name in the
map can never be applied to a table it does not fit.

Other oscillators that already carry a real name, oscillators holding
samples rather than wavetables, and tables with no counterpart in the
library are all left untouched.

--- Output ---

A summary to stdout, and with --report a TSV of every slot considered,
including the ones deliberately skipped and why.
"""

import argparse
import os
import re
import struct
import sys

FXP_HEADER = 60          # sizeof(fxChunkSetCustom)
PATCH_HEADER = 32        # sizeof(patch_header): 'sub3' + xmlsize + wtsize[2][3]
WT_HEADER = 12           # sizeof(wt_header)
N_SCENES, N_OSCS = 2, 3

WTF_IS_SAMPLE = 1
WTF_INT16 = 4
WTF_INT16_IS_16 = 8

PLACEHOLDERS = ("", "(Patch Wavetable)", "(Patch Sample)")
LSB = 1.0 / 16384.0

# A table that is bit-identical apart from this many samples is the same
# wavetable rendered slightly differently, typically at a waveform's
# discontinuity. Both limits must hold, so a small table cannot qualify on the
# fraction alone nor a large one on the absolute count alone.
MAX_OUTLIER_SAMPLES = 16
MAX_OUTLIER_FRACTION = 0.0005

# Wavetables removed from the factory set as byte-for-byte duplicates, mapped to
# the file that survives. A patch naming the retired file is repointed, after its
# embedded table is checked against the replacement.
RETIRED_NAMES = {
    "mw1": "VeryHi WM",     # identical to Waldorf/VeryHi WM.wt
}

# Slots identified by ear, where the numbers alone cannot make the call. Each is
# audibly the shipped wavetable but a different rendering of it, so the tables do
# not compare equal at any honest tolerance. Keyed by patch, scene and oscillator;
# the value is the wavetable name, why it cannot be matched automatically, and
# whether the table may also be re-embedded.
#
# Naming alone reattaches the oscillator to the file, so a viewer who jogs the
# wavetable away and back gets the file's version rather than the patch's.
# Re-embedding settles that by making the patch hold the file, at the cost of
# changing how the patch sounds today, so it is a listening decision taken per
# patch rather than a default. --skip-ear-matches declines the whole set and
# leaves those slots showing "(Patch Wavetable)".
EAR_MATCHES = {
    # Triangle frame 0 at half resolution: the patch holds 512 x 2 where the file
    # is 1024 x 16, the two frames are identical, and resampling the file's first
    # frame down to 512 reproduces the patch to within one step. Re-embedding was
    # auditioned and approved; it is audible only at very low octaves, and this
    # patch does not key track.
    ("patches_3rdparty/Argitoth/Drums/Kick.fxp", "A", 2):
        ("Triangle", "512 x 2 holding frame 0 of a 1024 x 16 file", True),
    # The same wavetable generated with an off-by-one. Its frames are the file's
    # function sampled over a 127 sample period inside a 128 sample frame, the
    # inclusive-endpoint mistake: frame 0 is cos(2*pi*n/127) where the file is
    # exactly cos(2*pi*n/128), and every frame's last sample duplicates its first,
    # which no frame of the file does. Audibly that is a one sample plateau at the
    # wrap and a small DC offset. Re-embedding therefore repairs the patch rather
    # than merely relabelling it, and was auditioned against the file.
    ("patches_3rdparty/Zoozither/Pads/Oncoming.fxp", "A", 2):
        ("Cosine Inverse Power", "generated on a 127 sample period, not 128", True),
}

# modsources that never swing below zero, by index into enum modsources
UNIPOLAR_SOURCES = {
    1,    # velocity
    2,    # keytrack
    6,    # modwheel
    15,   # amp EG
    16,   # filter EG
    29,   # timbre
    30,   # release velocity
    32,   # random unipolar
    34,   # alternate unipolar
    35,   # breath
    36,   # expression
    37,   # sustain
}
LFO_FIRST, LFO_LAST = 17, 22        # ms_lfo1 .. ms_lfo6
SLFO_FIRST, SLFO_LAST = 23, 28      # ms_slfo1 .. ms_slfo6


# ---------------------------------------------------------------- wavetables

def decode_samples(flags, n_samples, n_tables, raw):
    """Return the table as rows of floats, matching Wavetable::BuildWT."""
    count = n_samples * n_tables
    if flags & WTF_INT16:
        vals = struct.unpack("<%dh" % count, raw[:2 * count])
        scale = 1.0 / 32768.0 if (flags & WTF_INT16_IS_16) else 1.0 / 16384.0
        flat = [v * scale for v in vals]
    else:
        flat = list(struct.unpack("<%df" % count, raw[:4 * count]))
    return [flat[i * n_samples:(i + 1) * n_samples] for i in range(n_tables)]


def encode_for_patch(flags, n_samples, n_tables, raw):
    """
    Reproduce what save_patch writes for an embedded table: always int16, taken
    from Wavetable::TableI16. For an int16 source that is the file's own data;
    for a float source it is float2i15, which truncates toward zero.
    """
    if flags & WTF_INT16:
        return raw[:2 * n_samples * n_tables], flags | WTF_INT16
    count = n_samples * n_tables
    out = bytearray()
    for f in struct.unpack("<%df" % count, raw[:4 * count]):
        v = int(f * 16384.0)                    # the (int) cast truncates toward zero
        out += struct.pack("<h", max(-16384, min(16383, v)))
    return bytes(out), flags | WTF_INT16


def read_wt_file(path):
    with open(path, "rb") as fh:
        b = fh.read()
    if b[:4] != b"vawt":
        return None
    n_samples, = struct.unpack_from("<I", b, 4)
    n_tables, flags = struct.unpack_from("<HH", b, 8)
    if n_samples <= 0 or n_samples > 4096 or n_tables <= 0 or n_tables > 512:
        return None
    width = 2 if flags & WTF_INT16 else 4
    need = width * n_samples * n_tables
    raw = b[WT_HEADER:WT_HEADER + need]
    if len(raw) < need:                          # Surge's own loader zero-pads a short file
        raw += b"\0" * (need - len(raw))
    rows = decode_samples(flags, n_samples, n_tables, raw)
    return dict(path=path, n_samples=n_samples, n_tables=n_tables, flags=flags,
                raw=raw, rows=rows, i15=to_i15(rows))


SH_FOR_LEN = {1 << n: n for n in range(1, 13)}      # frame sizes 2 .. 4096


def read_wav_file(path):
    """
    Read a .wav the way SurgeStorage::load_wt_wav_portable does, so a .wav in the
    wavetable menu is compared as the table Surge would actually build from it.

    The frame size is metadata, not geometry: a clm or uhWT chunk means 2048, a
    regular cue chunk gives the spacing, srge carries it outright, and a smpl
    loop implies it. Without any of those the file is treated as a sample, which
    is not a wavetable match and is skipped here.
    """
    with open(path, "rb") as fh:
        b = fh.read()
    if len(b) < 12 or b[0:4] != b"RIFF" or b[8:12] != b"WAVE":
        return None

    fmt = None
    has_fmt = has_smpl = has_clm = has_cue = has_srge = False
    clm_len = smpl_len = cue_len = srge_len = 0
    wavdata, datasamples = None, 0

    off = 12
    while off + 8 <= len(b):
        ctype = b[off:off + 4]
        cs, = struct.unpack_from("<i", b, off + 4)
        off += 8
        if cs < 0:
            break
        if cs % 2 == 1:                              # RIFF chunks are word aligned
            cs += 1
        data = b[off:off + cs]
        if len(data) != cs:
            break
        off += cs

        if ctype == b"fmt ":
            if cs < 16:
                return None
            has_fmt = True
            fmt = struct.unpack_from("<HHIIHH", data, 0)
            audio_format, channels, bits = fmt[0], fmt[1], fmt[5]
            if not (((audio_format == 1 and bits == 16) or (audio_format == 3 and bits == 32))
                    and channels > 0):
                return None                          # Surge refuses these too
        elif ctype == b"clm ":
            if cs >= 7 and data[3:7] == b"2048":
                has_clm, clm_len = True, 2048
        elif ctype == b"uhWT":
            has_clm, clm_len = True, 2048
        elif ctype in (b"srge", b"srgo"):
            if cs >= 8:
                has_srge = ctype == b"srge" or has_srge
                srge_len, = struct.unpack_from("<i", data, 4)
        elif ctype == b"cue ":
            n_cues = struct.unpack_from("<i", data, 0)[0] if cs >= 4 else 0
            in_chunk = (cs - 4) // 24 if cs >= 4 else 0
            if n_cues < 0 or n_cues > in_chunk:
                n_cues = in_chunk
            starts = [struct.unpack_from("<6i", data, 4 + i * 24)[5] for i in range(n_cues)]
            d, regular = -1, True
            for i in range(1, len(starts)):
                if d == -1:
                    d = starts[i] - starts[i - 1]
                elif d != starts[i] - starts[i - 1]:
                    regular = False
            if regular:
                has_cue, cue_len = True, d
        elif ctype == b"data":
            if not has_fmt:
                return None
            datasamples = cs * 8 // fmt[5] // fmt[1]
            wavdata = data
        elif ctype == b"smpl":
            if cs < 36:
                continue
            sc = struct.unpack_from("<9I", data, 0)
            if sc[7] == 0:                           # RAPID writes an empty smpl to mean 2048
                has_smpl, smpl_len = True, 2048
            if sc[7] >= 1 and cs >= 60:
                loop = struct.unpack_from("<6I", data, 36)
                has_smpl = True
                smpl_len = loop[3] - loop[2] + 1 or 2048

    if not has_fmt or wavdata is None:
        return None

    loop_data = has_smpl or has_clm or has_srge
    loop_len = (clm_len if has_clm else
                cue_len if has_cue else
                srge_len if has_srge else
                smpl_len if has_smpl else -1)
    if loop_len in (0, -1):
        return None

    sh = SH_FOR_LEN.get(loop_len, 0) if loop_data else 0
    if sh == 0 or datasamples // loop_len < 1:
        return None                                  # a sample, not a wavetable

    audio_format, channels, bits = fmt[0], fmt[1], fmt[5]
    n_samples = 1 << sh
    sample_length = min(datasamples, 4096 * 512)
    n_tables = min(512, sample_length >> sh)
    if n_tables < 1:
        return None
    flags = WTF_INT16 if (audio_format == 1 and bits == 16) else 0

    step = bits // 8
    if channels > 1:                                 # Surge keeps the left channel
        mono = bytearray()
        for i in range(datasamples):
            src = i * step * channels
            mono += wavdata[src:src + step]
        wavdata = bytes(mono)

    need = n_samples * n_tables * step
    if len(wavdata) < need:
        wavdata += b"\0" * (need - len(wavdata))
    raw = wavdata[:need]
    rows = decode_samples(flags, n_samples, n_tables, raw)
    return dict(path=path, n_samples=n_samples, n_tables=n_tables, flags=flags, raw=raw,
                rows=rows, i15=to_i15(rows))


def load_library(data_dir):
    """
    Everything refresh_wtlist would put in the wavetable menu. That is .wt and
    .wav alike -- see supportedTableFileTypes in SurgeStorage.cpp -- and more than
    half the shipped library is .wav, so a matcher that reads only .wt silently
    fails to recognize the larger half of it.

    .wtscript is deliberately not covered. Those tables are produced by running
    Lua, which this script cannot do, so a patch holding one is reported as
    having no match rather than being matched wrongly.
    """
    lib = []
    for base in ("wavetables", "wavetables_3rdparty"):
        for dirpath, _, files in os.walk(os.path.join(data_dir, base)):
            for f in sorted(files):
                low = f.lower()
                full = os.path.join(dirpath, f)
                if low.endswith(".wt"):
                    wt = read_wt_file(full)
                elif low.endswith(".wav"):
                    wt = read_wav_file(full)
                else:
                    continue
                if wt is None:
                    continue
                wt["rel"] = os.path.relpath(full, data_dir).replace("\\", "/")
                wt["name"] = os.path.splitext(f)[0]
                lib.append(wt)
    return lib


def to_i15(rows):
    """
    Put a table through float2i15, the conversion save_patch applies on the way
    into an .fxp: truncate toward zero, then clamp to [-16384, 16383].

    Comparing in this space rather than in floats is not a tolerance, it is the
    only correct comparison. Several shipped wavetables reach past +/-1 -- Laser
    peaks at 1.676, Noise at 1.469 -- and the clamp means the copy inside a patch
    is legitimately a clipped version of the file. Compared as floats those look
    wildly different while being the same wavetable.
    """
    out = []
    for r in rows:
        row = []
        for x in r:
            v = int(x * 16384.0)
            row.append(-16384 if v < -16384 else (16383 if v > 16383 else v))
        out.append(row)
    return out


def max_i15_diff(a, b):
    """Largest difference, in int15 steps, between two already-quantized tables."""
    worst = 0
    for ra, rb in zip(a, b):
        for x, y in zip(ra, rb):
            d = x - y
            if d < 0:
                d = -d
            if d > worst:
                worst = d
    return worst


def i15_outliers(a, b):
    """
    (count, total) of samples differing by more than one step.

    The largest difference on its own is a poor test of sameness, because a
    waveform with a discontinuity - any sawtooth - can be rendered two ways that
    differ only in where the reset lands, and a single sample at the jump then
    reads as a near full scale difference. Organ 3 against Saw Asymmetric differs
    by 15879 steps at exactly one sample out of 16384, and by nothing anywhere
    else. Counting how many samples disagree separates that from a table which is
    genuinely different, where the disagreement is everywhere.
    """
    n = bad = 0
    for ra, rb in zip(a, b):
        for x, y in zip(ra, rb):
            n += 1
            d = x - y
            if d > 1 or d < -1:
                bad += 1
    return bad, n


def max_diff(rows_a, rows_b):
    worst = 0.0
    for ra, rb in zip(rows_a, rows_b):
        for x, y in zip(ra, rb):
            d = x - y
            if d < 0:
                d = -d
            if d > worst:
                worst = d
    return worst


# --------------------------------------------------------------------- patch

class PatchFile:
    """
    Reads an .fxp far enough to edit it, and writes it back with everything it
    did not touch preserved byte for byte -- including the arbitrary data block
    that follows the wavetable chunks.
    """

    def __init__(self, path):
        self.path = path
        self.ok = False
        with open(path, "rb") as fh:
            self.blob = fh.read()
        if len(self.blob) < FXP_HEADER + PATCH_HEADER:
            return
        if self.blob[0:4] != b"CcnK" or self.blob[8:12] != b"FPCh" or self.blob[16:20] != b"cjs3":
            return
        chunk_size, = struct.unpack_from(">I", self.blob, 56)
        data = self.blob[FXP_HEADER:FXP_HEADER + chunk_size]
        if len(data) < PATCH_HEADER or data[0:4] != b"sub3":
            return
        xmlsize, = struct.unpack_from("<I", data, 4)
        wtsize = struct.unpack_from("<6I", data, 8)
        off = PATCH_HEADER
        if off + xmlsize > len(data):
            return
        self.xml = data[off:off + xmlsize].decode("utf-8", "replace")
        off += xmlsize
        self.chunks = {}
        for sc in range(N_SCENES):
            for osc in range(N_OSCS):
                size = wtsize[sc * N_OSCS + osc]
                if not size:
                    self.chunks[(sc, osc)] = None
                    continue
                blk = data[off:off + size]
                off += size
                if len(blk) < WT_HEADER:
                    return
                ns, = struct.unpack_from("<I", blk, 4)
                nt, flags = struct.unpack_from("<HH", blk, 8)
                width = 2 if flags & WTF_INT16 else 4
                need = width * ns * nt
                raw = blk[WT_HEADER:WT_HEADER + need]
                if len(raw) < need or ns == 0 or nt == 0:
                    return
                self.chunks[(sc, osc)] = dict(n_samples=ns, n_tables=nt, flags=flags, raw=raw)
        self.arbdata = data[off:]
        self.ok = True

    # -- XML surgery. Deliberately textual: re-serializing through an XML writer
    # -- would reflow every element and bury the real change in noise.

    def _osc_extra(self, scene, osc):
        return re.search(r'<osc_extra_sc%d_osc%d\b[^>]*>' % (scene, osc), self.xml)

    def display_name(self, scene, osc):
        m = self._osc_extra(scene, osc)
        if m is None:
            return None
        a = re.search(r'wavetable_display_name="([^"]*)"', m.group(0))
        return a.group(1) if a else None

    def set_display_name(self, scene, osc, name):
        esc = (name.replace("&", "&amp;").replace("<", "&lt;")
                   .replace(">", "&gt;").replace('"', "&quot;"))
        attr = 'wavetable_display_name="%s"' % esc
        m = self._osc_extra(scene, osc)
        if m is not None:
            tag = m.group(0)
            if 'wavetable_display_name="' in tag:
                new = re.sub(r'wavetable_display_name="[^"]*"', attr, tag)
            elif tag.endswith("/>"):
                new = tag[:-2].rstrip() + " " + attr + " />"
            else:
                new = tag[:-1].rstrip() + " " + attr + ">"
            self.xml = self.xml[:m.start()] + new + self.xml[m.end():]
            return
        # No element for this oscillator. Add one, creating <extraoscdata> if needed.
        # load_xml keys on the scene and osc attributes, not on the tag name, but
        # the tag is spelled the way save_xml spells it so a re-save is a no-op.
        elem = ('<osc_extra_sc%d_osc%d scene="%d" osc="%d" %s extra_n="0" />'
                % (scene, osc, scene, osc, attr))
        if "<extraoscdata" in self.xml:
            self.xml = self.xml.replace("</extraoscdata>", elem + "</extraoscdata>", 1)
        else:
            self.xml = self.xml.replace("</patch>",
                                        "<extraoscdata>" + elem + "</extraoscdata></patch>", 1)

    def _param0(self, scene, osc):
        tag = ("a" if scene == 0 else "b") + "_osc%d_param0" % (osc + 1)
        m = re.search(r'<%s\b[^>]*?/>' % tag, self.xml)
        if m:
            return m
        return re.search(r'<%s\b[^>]*?>.*?</%s>' % (tag, tag), self.xml, re.S)

    def lfo_is_unipolar(self, scene, index):
        """index is 1..6 for a voice LFO, 7..12 for a scene LFO."""
        tag = ("a" if scene == 0 else "b") + "_lfo%d_unipolar" % index
        m = re.search(r'<%s\b[^>]*>' % tag, self.xml)
        if not m:
            return False
        v = re.search(r'value="([^"]*)"', m.group(0))
        return bool(v) and float(v.group(1)) >= 0.5

    def morph(self, scene, osc):
        """Return (value, extend_range, [(source, depth), ...]) or (None, False, [])."""
        m = self._param0(scene, osc)
        if m is None:
            return None, False, []
        text = m.group(0)
        head = text[:text.index(">") + 1]
        val = re.search(r'\bvalue="([^"]*)"', head)
        ext = re.search(r'\bextend_range="([^"]*)"', head)
        mods = []
        for mm in re.finditer(r'<modrouting\b[^>]*>', text[len(head):]):
            src = re.search(r'source="([^"]*)"', mm.group(0))
            dep = re.search(r'depth="([^"]*)"', mm.group(0))
            muted = re.search(r'muted="([^"]*)"', mm.group(0))
            if src and dep and not (muted and muted.group(1) not in ("0", "false")):
                mods.append((int(src.group(1)), float(dep.group(1))))
        return (float(val.group(1)) if val else None,
                bool(ext and ext.group(1) not in ("0", "false")),
                mods)

    def morph_ceiling_reach(self, scene, osc):
        """Highest Morph value the routings can drive this oscillator to."""
        value, _, mods = self.morph(scene, osc)
        if value is None:
            return 0.0
        hi = value
        for src, depth in mods:
            unipolar = src in UNIPOLAR_SOURCES
            if LFO_FIRST <= src <= LFO_LAST:
                unipolar = self.lfo_is_unipolar(scene, src - LFO_FIRST + 1)
            elif SLFO_FIRST <= src <= SLFO_LAST:
                unipolar = self.lfo_is_unipolar(scene, src - SLFO_FIRST + 7)
            hi += max(0.0, depth) if unipolar else abs(depth)
        return hi

    def scale_morph(self, scene, osc, k):
        m = self._param0(scene, osc)
        text = m.group(0)
        cut = text.index(">") + 1
        head, tail = text[:cut], text[cut:]
        head = re.sub(r'\bvalue="([^"]*)"',
                      lambda mm: 'value="%.6f"' % (float(mm.group(1)) * k), head, count=1)
        tail = re.sub(r'<modrouting\b[^>]*>',
                      lambda mm: re.sub(r'\bdepth="([^"]*)"',
                                        lambda d: 'depth="%.6f"' % (float(d.group(1)) * k),
                                        mm.group(0)),
                      tail)
        self.xml = self.xml[:m.start()] + head + tail + self.xml[m.end():]

    def replace_table(self, scene, osc, wt):
        raw, flags = encode_for_patch(wt["flags"], wt["n_samples"], wt["n_tables"], wt["raw"])
        self.chunks[(scene, osc)] = dict(n_samples=wt["n_samples"], n_tables=wt["n_tables"],
                                         flags=flags, raw=raw)

    def serialize(self):
        xml = self.xml.encode("utf-8")
        sizes, bodies = [], []
        for sc in range(N_SCENES):
            for osc in range(N_OSCS):
                c = self.chunks[(sc, osc)]
                if c is None:
                    sizes.append(0)
                    continue
                # save_patch zeroes the tag of an embedded table. Flags are written back
                # exactly as they were read: a chunk this script did not replace must come
                # out byte for byte as it went in, and a handful of old patches hold float
                # data with no int16 flag. replace_table sets the flag where it applies.
                body = (b"\0\0\0\0"
                        + struct.pack("<I", c["n_samples"])
                        + struct.pack("<HH", c["n_tables"], c["flags"])
                        + c["raw"])
                sizes.append(len(body))
                bodies.append(body)
        data = (b"sub3" + struct.pack("<I", len(xml)) + struct.pack("<6I", *sizes)
                + xml + b"".join(bodies) + self.arbdata)
        head = bytearray(self.blob[:FXP_HEADER])
        struct.pack_into(">I", head, 56, len(data))     # chunkSize; byteSize is left as written
        return bytes(head) + data


# ------------------------------------------------------------------ matching

def classify(chunk, lib, near_lsb):
    """Return (kind, wavetable, detail), kind in exact / near / prehdr / none."""
    rows = to_i15(decode_samples(chunk["flags"], chunk["n_samples"], chunk["n_tables"],
                                 chunk["raw"]))
    same_dim = [w for w in lib
                if w["n_samples"] == chunk["n_samples"] and w["n_tables"] == chunk["n_tables"]]
    scored = sorted(((max_i15_diff(rows, w["i15"]), w) for w in same_dim),
                    key=lambda t: (t[0], t[1]["rel"]))
    if scored:
        best_d, best = scored[0]
        if best_d <= 1:
            ties = [w["rel"] for d, w in scored if d <= 1]
            return "exact", best, dict(lsb=best_d, ties=ties)
        if best_d <= near_lsb:
            return "near", best, dict(lsb=best_d, ties=[])
        # A handful of disagreeing samples in an otherwise bit-identical table is
        # a rendering difference at a discontinuity, not a different wavetable.
        for d, w in scored:
            bad, total = i15_outliers(rows, w["i15"])
            if bad <= MAX_OUTLIER_SAMPLES and bad <= total * MAX_OUTLIER_FRACTION:
                return "sparse", w, dict(lsb=d, ties=[], outliers=bad, total=total)

    # leading-frame match: the pre-#8440 wave count case
    for w in sorted(lib, key=lambda w: w["rel"]):
        if w["n_samples"] != chunk["n_samples"] or w["n_tables"] <= chunk["n_tables"]:
            continue
        d = max_i15_diff(rows, w["i15"][:chunk["n_tables"]])
        if d <= 2:
            return "prehdr", w, dict(lsb=d, ties=[])
    return "none", None, dict(lsb=scored[0][0] if scored else None, ties=[])


def span(n_tables, extend):
    return n_tables - 1 + (0 if extend else 1)


# ---------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(
        description="Name patch-embedded wavetables in Surge XT factory and 3rd party patches.")
    ap.add_argument("--data-dir", default="resources/data",
                    help="path to resources/data (default: %(default)s)")
    ap.add_argument("--apply", action="store_true",
                    help="write the patches; without this the script only reports")
    ap.add_argument("--near-lsb", type=float, default=8.0,
                    help="tolerance for a near match, in int15 LSB (default: %(default)s)")
    ap.add_argument("--skip-names", action="store_true",
                    help="skip the clean renames, do only the pre-#8440 group")
    ap.add_argument("--skip-prehdr", action="store_true",
                    help="skip the pre-#8440 group, do only the clean renames")
    ap.add_argument("--skip-ear-matches", action="store_true",
                    help="do not apply the curated EAR_MATCHES names, leaving those slots "
                         "showing the placeholder")
    ap.add_argument("--hold-ceiling-clippers", action="store_true",
                    help="leave pre-#8440 slots whose Morph modulation reaches the ceiling alone, "
                         "and report them instead of correcting them")
    ap.add_argument("--prefer", action="append", default=[], metavar="SUBSTR",
                    help="on an ambiguous match prefer the wavetable whose path contains SUBSTR; "
                         "repeatable")
    ap.add_argument("--report", metavar="TSV", help="write a per-slot TSV report here")
    args = ap.parse_args()

    if not os.path.isdir(args.data_dir):
        sys.exit("error: %s not found. Run this from the Surge XT repository root, or pass "
                 "--data-dir." % args.data_dir)

    lib = load_library(args.data_dir)
    if not lib:
        sys.exit("error: no .wt files found under %s" % args.data_dir)
    print("wavetable library: %d files" % len(lib))

    report, counts = [], dict(exact=0, near=0, prehdr=0, none=0, named=0,
                              sample=0, ear=0, ambiguous=0, retired=0,
                              ear_match=0, sparse=0)
    changed = 0

    for base in ("patches_factory", "patches_3rdparty"):
        for dirpath, _, files in os.walk(os.path.join(args.data_dir, base)):
            for f in sorted(files):
                if not f.lower().endswith(".fxp"):
                    continue
                full = os.path.join(dirpath, f)
                rel = os.path.relpath(full, args.data_dir).replace("\\", "/")
                p = PatchFile(full)
                if not p.ok:
                    print("  skipped, could not parse: %s" % rel)
                    report.append((rel, "", "skip", "unparseable", "", "", ""))
                    continue

                dirty = False
                for sc in range(N_SCENES):
                    for osc in range(N_OSCS):
                        chunk = p.chunks[(sc, osc)]
                        if chunk is None:
                            continue
                        slot = "%s/%d" % ("AB"[sc], osc + 1)

                        if chunk["flags"] & WTF_IS_SAMPLE:
                            counts["sample"] += 1
                            report.append((rel, slot, "skip", "holds a sample", "", "", ""))
                            continue
                        current = p.display_name(sc, osc)
                        if current in RETIRED_NAMES:
                            want = RETIRED_NAMES[current]
                            repl = next((w for w in lib if w["name"] == want), None)
                            rows = to_i15(decode_samples(chunk["flags"], chunk["n_samples"],
                                                         chunk["n_tables"], chunk["raw"]))
                            if repl is None:
                                print("  retired name %r maps to %r, which is not in the library"
                                      % (current, want))
                            elif (repl["n_samples"] != chunk["n_samples"]
                                    or repl["n_tables"] != chunk["n_tables"]
                                    or max_i15_diff(rows, repl["i15"]) > 1):
                                print("  %s %s names retired %r but its table does not match %r; "
                                      "left alone" % (rel, slot, current, want))
                            else:
                                counts["retired"] += 1
                                report.append((rel, slot, "retired", repl["rel"], want,
                                               "was %s" % current, ""))
                                p.set_display_name(sc, osc, want)
                                dirty = True
                            continue
                        if current is not None and current not in PLACEHOLDERS:
                            counts["named"] += 1
                            report.append((rel, slot, "skip", "already named", current, "", ""))
                            continue

                        kind, wt, detail = classify(chunk, lib, args.near_lsb)
                        if kind == "none":
                            ear = EAR_MATCHES.get((rel, "AB"[sc], osc + 1))
                            if ear and not args.skip_ear_matches:
                                want, why, reimport = ear
                                repl = next((w for w in lib if w["name"] == want), None)
                                if repl is None:
                                    print("  ear match %r for %s %s is not in the library"
                                          % (want, rel, slot))
                                else:
                                    counts["ear_match"] += 1
                                    morph = ""
                                    p.set_display_name(sc, osc, want)
                                    if reimport:
                                        value, extend, _ = p.morph(sc, osc)
                                        if value is not None:
                                            k = (span(chunk["n_tables"], extend)
                                                 / float(span(repl["n_tables"], extend)))
                                            p.scale_morph(sc, osc, k)
                                            morph = "%.6f -> %.6f" % (value, value * k)
                                        p.replace_table(sc, osc, repl)
                                        why = why + "; re-embedded"
                                    report.append((rel, slot, "ear", repl["rel"], want,
                                                   morph, why))
                                    dirty = True
                                continue
                            counts["none"] += 1
                            report.append((rel, slot, "skip", "no library match", "", "",
                                           "" if detail["lsb"] is None
                                           else "closest same-size %d steps" % detail["lsb"]))
                            continue

                        if len(detail["ties"]) > 1:
                            counts["ambiguous"] += 1
                            for pref in args.prefer:
                                hit = next((t for t in detail["ties"] if pref in t), None)
                                if hit:
                                    wt = next(w for w in lib if w["rel"] == hit)
                                    break
                            print("  ambiguous: %s %s matches %s -- using %s"
                                  % (rel, slot, ", ".join(detail["ties"]), wt["rel"]))

                        if kind in ("exact", "near", "sparse"):
                            if args.skip_names:
                                continue
                            counts[kind] += 1
                            note = ""
                            if kind == "sparse":
                                note = ("%d of %d samples differ"
                                        % (detail["outliers"], detail["total"]))
                            report.append((rel, slot, kind, wt["rel"], wt["name"],
                                           "%d steps" % detail["lsb"], note))
                            p.set_display_name(sc, osc, wt["name"])
                            dirty = True
                            continue

                        if args.skip_prehdr:
                            continue
                        value, extend, _ = p.morph(sc, osc)
                        if value is None:
                            report.append((rel, slot, "skip", "no Morph parameter found",
                                           wt["rel"], "", ""))
                            continue
                        reach = p.morph_ceiling_reach(sc, osc)
                        if reach > 1.0 + 1e-9 and args.hold_ceiling_clippers:
                            counts["ear"] += 1
                            report.append((rel, slot, "skip",
                                           "held back: Morph modulation reaches the ceiling",
                                           wt["rel"], "%.6f" % value, "reaches %.3f" % reach))
                            continue
                        k = span(chunk["n_tables"], extend) / float(span(wt["n_tables"], extend))
                        counts["prehdr"] += 1
                        report.append((rel, slot, "prehdr", wt["rel"], wt["name"],
                                       "%.6f -> %.6f" % (value, value * k),
                                       "%d -> %d frames" % (chunk["n_tables"], wt["n_tables"])))
                        p.set_display_name(sc, osc, wt["name"])
                        p.scale_morph(sc, osc, k)
                        p.replace_table(sc, osc, wt)
                        dirty = True

                if dirty:
                    changed += 1
                    if args.apply:
                        out = p.serialize()
                        with open(full, "wb") as fh:
                            fh.write(out)
                        check = PatchFile(full)
                        if not check.ok:
                            sys.exit("error: rewriting %s produced an unreadable patch" % rel)

    print()
    print("clean renames, identical            : %d" % counts["exact"])
    print("clean renames, near (<= %-4.0f LSB)    : %d" % (args.near_lsb, counts["near"]))
    print("clean renames, all but a few samples : %d" % counts["sparse"])
    print("pre-#8440 name + reimport + Morph   : %d" % counts["prehdr"])
    print("repointed off a retired wavetable   : %d" % counts["retired"])
    print("named from the curated ear list     : %d" % counts["ear_match"])
    if args.hold_ceiling_clippers:
        print("held back, ceiling clippers         : %d" % counts["ear"])
    print("left alone, already named           : %d" % counts["named"])
    print("left alone, holds a sample          : %d" % counts["sample"])
    print("left alone, no library match        : %d" % counts["none"])
    if counts["ambiguous"]:
        print("ambiguous matches                   : %d  (resolve with --prefer)"
              % counts["ambiguous"])
    print()
    print("%s %d patch file(s)" % ("rewrote" if args.apply else "would rewrite", changed))
    if not args.apply:
        print("run again with --apply to write the changes")

    if args.report:
        with open(args.report, "w", encoding="utf-8") as fh:
            fh.write("patch\tslot\taction\tdetail\tname\tmorph\tnotes\n")
            for row in report:
                fh.write("\t".join(str(x) for x in row) + "\n")
        print("report written to %s" % args.report)


if __name__ == "__main__":
    main()
