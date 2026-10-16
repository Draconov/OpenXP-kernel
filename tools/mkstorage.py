#!/usr/bin/env python3
"""Explicitly create a blank OpenXP-only secondary IDE disk image.
NEVER run this tool against an existing disk. It refuses an existing path.
"""
import argparse
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("path", help="new disk image path (must not exist)")
parser.add_argument("--legacy-v1", action="store_true", help="explicitly create old 12-file geometry")
args = parser.parse_args()
path = Path(args.path)
header = bytearray(512)
header[:8] = b"OXPDISK1" if args.legacy_v1 else b"OXPDISK2"
struct.pack_into("<I", header, 8, 1 if args.legacy_v1 else 2)
with path.open("xb") as f:
    f.write(header)
    f.truncate(2 * 1024 * 1024)
print(f"Created dedicated OpenXP data image: {path} (2 MiB, journal v{1 if args.legacy_v1 else 2})")
