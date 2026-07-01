#!/usr/bin/env python3
"""
fix_wt_wavecount.py

Validates Surge .wt wavetable files against the format spec, and fixes
a specific class of corruption: a wave_count header field that doesn't
match the amount of wave data actually present in the file.

--- .wt format (from WT_fileformat.txt) ---
bytes 0-3   'vawt' magic (big-endian text; everything after is little-endian)
bytes 4-7   wave_size   (uint32) samples per wave; power of 2, 2..4096
bytes 8-9   wave_count  (uint16) number of waves; 1..512
bytes 10-11 flags       (uint16) bit 0001 sample, 0002 looped, 0004 int16
                         format (else float32), 0008 full-range int16
bytes 12-.. wave data:  bytes_per_sample * wave_size * wave_count
                         (bytes_per_sample = 2 if int16 flag set, else 4)

--- What this script checks ---
1. Magic bytes correct ('vawt')
2. wave_size is a power of 2 in [2, 4096]
3. wave_count in [1, 512]
4. flags has no unexpected/reserved bits set
5. declared total size (12 + bytes_per_sample*wave_size*wave_count)
   matches the actual file size

--- What this script fixes (two distinct, unambiguous cases) ---

Case A -- header UNDERCOUNTS real data (grow wave_count):
If the file has MORE data than declared, and that excess divides evenly
by (bytes_per_sample * wave_size), the extra data is exactly whole
additional wave frame(s). The header's wave_count is patched upward to
the true count.

Case B -- trailing garbage appended after otherwise-valid data (truncate):
If the file has MORE data than declared, but the excess does NOT divide
evenly by (bytes_per_sample * wave_size), that excess cannot represent
whole additional frames -- it's leftover bytes tacked onto an otherwise
self-consistent, valid header (this is commonly leftover/uninitialized
buffer memory from whatever tool wrote the file; for float32 data this
is confirmed if the trailing floats contain NaN/Inf values). The file
is truncated back down to exactly the size the header declares, and
nothing else is touched.

Case C -- file has LESS data than declared (zero-padded, matches Surge's own runtime behavior):
If the file is smaller than the header declares, Surge's own .wt loader
already handles this at load time by zero-padding the missing tail of
the read buffer (see the loader's short-read fallback) rather than
erroring. This script does the same thing at rest: the file is padded
with zero bytes up to the declared size. wave_size/wave_count/flags
are left untouched -- only zero bytes are appended.
Use --no-zero-pad to instead just report shortfalls without touching them.

Issues with the header itself (bad magic, invalid wave_size, invalid
wave_count, unexpected flag bits) are always reported only.

Usage:
    python3 fix_wt_wavecount.py                                  # recurse *.wt in cwd
    python3 fix_wt_wavecount.py file1.wt file2.wt ...
    python3 fix_wt_wavecount.py --dir /path/to/folder
    python3 fix_wt_wavecount.py --dry-run                        # report only, no changes
    python3 fix_wt_wavecount.py --no-zero-pad                    # leave shortfalls unpadded
    python3 fix_wt_wavecount.py --no-backup

By default, a backup copy of each modified file is written alongside it
with a ".bak" suffix before patching.
"""

import argparse
import math
import struct
import sys
from pathlib import Path

KNOWN_FLAG_BITS = 0x0001 | 0x0002 | 0x0004 | 0x0008


def is_power_of_two(n: int) -> bool:
    return n > 0 and (n & (n - 1)) == 0


def check_and_fix(path: Path, dry_run: bool, make_backup: bool, zero_pad: bool) -> str:
    data = path.read_bytes()
    size = len(data)

    if size < 12:
        return f"SKIP (file too small to contain a .wt header: {size} bytes)"
    if data[0:4] != b"vawt":
        return f"SKIP (bad magic bytes: {data[0:4]!r}, expected b'vawt')"

    wave_size, wave_count, flags = struct.unpack("<IHH", data[4:12])
    lines = []
    structural_problem = False

    if not is_power_of_two(wave_size) or not (2 <= wave_size <= 4096):
        lines.append(f"INVALID wave_size={wave_size} (must be a power of 2 in [2, 4096])")
        structural_problem = True

    if not (1 <= wave_count <= 512):
        lines.append(f"INVALID wave_count={wave_count} (must be in [1, 512])")
        structural_problem = True

    unknown_bits = flags & ~KNOWN_FLAG_BITS
    if unknown_bits:
        lines.append(f"unexpected flag bits set: 0x{unknown_bits:04x} (flags=0x{flags:04x})")

    is_int16 = bool(flags & 0x0004)
    bytes_per_sample = 2 if is_int16 else 4

    if structural_problem:
        # wave_size/wave_count out of spec -- don't attempt any size math or fix
        lines.append("size validation SKIPPED due to invalid header field(s) above")
        return "\n  ".join(lines)

    declared_data_bytes = bytes_per_sample * wave_size * wave_count
    declared_total = 12 + declared_data_bytes
    actual_data_bytes = size - 12
    diff = actual_data_bytes - declared_data_bytes

    if diff == 0:
        lines.append(
            f"OK: wave_size={wave_size}, wave_count={wave_count}, "
            f"format={'int16' if is_int16 else 'float32'}, size matches ({size} bytes)"
        )
        return "\n  ".join(lines)

    unit = bytes_per_sample * wave_size

    if diff > 0:
        # File has MORE data than the header declares.
        if diff % unit == 0:
            # Case A: excess is whole additional frame(s) -- header undercounts real data.
            implied_wave_count = actual_data_bytes // unit
            status = (
                f"SIZE MISMATCH: header declares wave_count={wave_count} "
                f"(expects {declared_total} bytes total), actual file is {size} bytes "
                f"({actual_data_bytes} bytes of wave data) -> actual data supports exactly "
                f"wave_count={implied_wave_count}"
            )
            if 1 <= implied_wave_count <= 512:
                if dry_run:
                    lines.append(status + " [dry-run, not fixed]")
                else:
                    if make_backup:
                        backup_path = path.with_suffix(path.suffix + ".bak")
                        if not backup_path.exists():
                            backup_path.write_bytes(data)
                    fixed = bytearray(data)
                    fixed[8:10] = struct.pack("<H", implied_wave_count)
                    path.write_bytes(fixed)
                    lines.append(status + " -> FIXED (wave_count increased)")
            else:
                lines.append(status + f", but that's out of valid range [1, 512], NOT fixed")
        else:
            # Case B: excess doesn't form whole frame(s) -- trailing garbage on an
            # otherwise self-consistent, valid header. Truncate back to declared size.
            trailing = data[12 + declared_data_bytes:]
            garbage_note = ""
            if not is_int16 and len(trailing) >= 4:
                n = len(trailing) // 4
                floats = struct.unpack(f"<{n}f", trailing[:n * 4])
                bad = sum(1 for f in floats if math.isnan(f) or math.isinf(f))
                if bad:
                    garbage_note = f"; trailing region contains {bad}/{n} NaN/Inf float values, confirms garbage"
            status = (
                f"TRAILING GARBAGE: header (wave_size={wave_size}, wave_count={wave_count}) is internally "
                f"valid and declares {declared_total} bytes, but file is {size} bytes -- {diff} extra byte(s) "
                f"appended that don't form a whole additional frame{garbage_note}"
            )
            if dry_run:
                lines.append(status + " [dry-run, not truncated]")
            else:
                if make_backup:
                    backup_path = path.with_suffix(path.suffix + ".bak")
                    if not backup_path.exists():
                        backup_path.write_bytes(data)
                path.write_bytes(data[:declared_total])
                lines.append(status + " -> FIXED (truncated to declared size)")
    else:
        # File has LESS data than the header declares. Surge's own .wt loader
        # already zero-pads this exact scenario at load time rather than
        # erroring, so we do the same thing at rest by default.
        status = (
            f"DATA SHORTFALL: header declares wave_count={wave_count} "
            f"(expects {declared_total} bytes total), but actual file is only {size} bytes "
            f"({actual_data_bytes} bytes of wave data, missing {-diff} byte(s))"
        )
        if not zero_pad:
            lines.append(status + " -- NOT fixed (--no-zero-pad)")
        elif dry_run:
            lines.append(status + f" -> would zero-pad {-diff} byte(s) [dry-run, not fixed]")
        else:
            if make_backup:
                backup_path = path.with_suffix(path.suffix + ".bak")
                if not backup_path.exists():
                    backup_path.write_bytes(data)
            padded = data + b"\x00" * (-diff)
            path.write_bytes(padded)
            lines.append(status + f" -> FIXED (zero-padded {-diff} byte(s) to match declared size)")

    return "\n  ".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*", help=".wt files to check/fix")
    ap.add_argument("--dir", default=".",
                     help="Recursively scan this directory for *.wt files (default: current directory)")
    ap.add_argument("--dry-run", action="store_true", help="Report issues without modifying files")
    ap.add_argument("--no-zero-pad", action="store_true",
                     help="Do not zero-pad data shortfalls; report them only instead")
    ap.add_argument("--no-backup", action="store_true", help="Do not write .bak backup copies")
    args = ap.parse_args()

    targets = [Path(f) for f in args.files]
    if not args.files:
        targets.extend(sorted(p for p in Path(args.dir).rglob("*") if p.suffix.lower() == ".wt"))
    elif args.dir != ".":
        targets.extend(sorted(p for p in Path(args.dir).rglob("*") if p.suffix.lower() == ".wt"))

    if not targets:
        ap.print_help()
        sys.exit(1)

    fixed_count = 0
    unresolved_count = 0
    for p in targets:
        if not p.is_file():
            print(f"{p}: SKIP (not found)")
            continue
        result = check_and_fix(p, dry_run=args.dry_run, make_backup=not args.no_backup,
                                zero_pad=not args.no_zero_pad)
        if "-> FIXED" in result:
            fixed_count += 1
        elif "SIZE MISMATCH" in result or "TRAILING GARBAGE" in result or "DATA SHORTFALL" in result or "INVALID" in result:
            unresolved_count += 1
        print(f"{p}:\n  {result}\n")

    print(f"Summary: {len(targets)} file(s) scanned.")
    print(f"         {fixed_count} fixed{' (would be fixed, dry-run)' if args.dry_run else ''}.")
    print(f"         {unresolved_count} with unresolved/structural issues needing manual review.")


if __name__ == "__main__":
    main()
