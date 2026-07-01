#!/usr/bin/env python3
"""
fix_riff_size.py

Scans WAV files for an incorrect outer RIFF chunk size field and fixes it,
and optionally performs a full read-only validation of the chunk structure.

--- RIFF size fix (always performed unless --dry-run) ---
Per the RIFF spec, the 4-byte size field right after "RIFF" must equal
(total file size - 8) -- i.e. everything AFTER that size field, including
the "WAVE" tag and all subchunks.

Many WAV writers get this wrong (off by 2, 4, 8, or arbitrary amounts,
often because of confusion over whether the size field's own 4 bytes,
the "WAVE" tag, or padding should be included). Most players/parsers
ignore this field and just walk subchunks until EOF, which is why such
files can still "sound correct" while being spec-invalid.

This is the ONLY thing this script ever modifies -- bytes 4-7 of the
file, and only if they don't match (file_size - 8). No chunk data, no
chunk headers, no padding bytes are ever touched.

--- Full chunk validation (report-only, never auto-fixed) ---
Enabled automatically by --dry-run, or explicitly via --validate on a
real run. This walks every subchunk (fmt, data, custom chunks like
"srge", etc.) from byte 12 to EOF and checks for:
  - truncated chunk headers (fewer than 8 bytes remaining)
  - a chunk body that would extend past EOF
  - a chunk with an odd size that's missing its required pad byte
  - trailing bytes after the last chunk that don't form another chunk
Any of these indicate real structural corruption (not just a header
miscount) and are reported only -- fixing them correctly depends on
what's actually wrong, so this script won't guess.

Usage:
    python3 fix_riff_size.py                                   # recurse *.wav in current directory
    python3 fix_riff_size.py file1.wav file2.wav ...
    python3 fix_riff_size.py --dir /path/to/folder              # recurse *.wav in a specific folder
    python3 fix_riff_size.py --dry-run                          # report only, full validation, cwd
    python3 fix_riff_size.py --dir /path/to/folder --validate   # fix RIFF size AND report chunk validation
    python3 fix_riff_size.py --dir /path/to/folder --no-backup

By default, a backup copy of each modified file is written alongside it
with a ".bak" suffix before patching.
"""

import argparse
import struct
import sys
from pathlib import Path


def validate_chunks(data: bytes) -> list:
    """Walk all subchunks from byte 12 to EOF, return a list of issue strings.
    Empty list means the chunk structure looks internally consistent."""
    size = len(data)
    issues = []
    pos = 12
    chunk_num = 0

    while pos < size:
        chunk_num += 1
        if pos + 8 > size:
            issues.append(
                f"chunk #{chunk_num} at offset {pos}: truncated header "
                f"({size - pos} bytes remaining, need 8)"
            )
            break

        cid, csize = struct.unpack("<4sI", data[pos:pos + 8])
        try:
            cid_str = cid.decode("ascii")
        except UnicodeDecodeError:
            cid_str = repr(cid)

        body_start = pos + 8
        body_end = body_start + csize

        if body_end > size:
            issues.append(
                f"chunk #{chunk_num} '{cid_str}' at offset {pos}: declared size {csize} "
                f"extends {body_end - size} bytes past EOF"
            )
            break

        pad = csize % 2
        if pad:
            if body_end >= size:
                issues.append(
                    f"chunk #{chunk_num} '{cid_str}' at offset {pos}: odd size ({csize}) "
                    f"but no room for required pad byte before EOF"
                )
                break
            pad_byte = data[body_end]
            if pad_byte != 0:
                issues.append(
                    f"chunk #{chunk_num} '{cid_str}' at offset {pos}: odd size ({csize}), "
                    f"pad byte at {body_end} is 0x{pad_byte:02x} (expected 0x00, non-fatal)"
                )

        pos = body_end + pad

    if pos == size and chunk_num == 0:
        issues.append("no subchunks found after the RIFF/WAVE header")
    elif 0 < pos < size:
        issues.append(f"{size - pos} trailing byte(s) after the last chunk (offset {pos} to {size})")

    return issues


def check_and_fix(path: Path, dry_run: bool, make_backup: bool, do_validate: bool) -> str:
    data = path.read_bytes()
    size = len(data)

    if size < 12 or data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        return "SKIP (not a RIFF/WAVE file)"

    declared = struct.unpack("<I", data[4:8])[0]
    correct = size - 8

    lines = []

    if declared == correct:
        lines.append(f"RIFF size OK (declared={declared})")
    else:
        diff = declared - correct
        riff_status = f"RIFF size BAD (declared={declared}, correct={correct}, off by {diff:+d} bytes)"
        if dry_run:
            lines.append(riff_status + " [dry-run, not fixed]")
        else:
            if make_backup:
                backup_path = path.with_suffix(path.suffix + ".bak")
                if not backup_path.exists():
                    backup_path.write_bytes(data)
            fixed = bytearray(data)
            fixed[4:8] = struct.pack("<I", correct)
            path.write_bytes(fixed)
            lines.append(riff_status + " -> FIXED")

    if do_validate:
        chunk_issues = validate_chunks(data)
        if chunk_issues:
            lines.append("CHUNK VALIDATION ISSUES:")
            lines.extend(f"    - {issue}" for issue in chunk_issues)
        else:
            lines.append("chunk structure OK (all subchunks consistent, no trailing bytes)")

    return "\n  ".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*", help="WAV files to check/fix")
    ap.add_argument("--dir", default=".",
                     help="Recursively scan this directory for *.wav files (default: current directory)")
    ap.add_argument("--dry-run", action="store_true",
                     help="Report issues without modifying files. Always includes full chunk validation.")
    ap.add_argument("--validate", action="store_true",
                     help="Also run full chunk validation on a real (non-dry-run) fix pass.")
    ap.add_argument("--no-backup", action="store_true", help="Do not write .bak backup copies")
    args = ap.parse_args()

    targets = [Path(f) for f in args.files]
    if args.dir:
        targets.extend(sorted(Path(args.dir).rglob("*.wav")))

    if not targets:
        ap.print_help()
        sys.exit(1)

    do_validate = args.dry_run or args.validate

    bad_riff = 0
    bad_chunks = 0
    for p in targets:
        if not p.is_file():
            print(f"{p}: SKIP (not found)")
            continue
        result = check_and_fix(p, dry_run=args.dry_run, make_backup=not args.no_backup, do_validate=do_validate)
        if "RIFF size BAD" in result:
            bad_riff += 1
        if "CHUNK VALIDATION ISSUES" in result:
            bad_chunks += 1
        print(f"{p}:\n  {result}\n")

    print(f"Summary: {bad_riff} of {len(targets)} file(s) had an incorrect RIFF size.")
    if do_validate:
        print(f"         {bad_chunks} of {len(targets)} file(s) had chunk-level structural issues.")


if __name__ == "__main__":
    main()
