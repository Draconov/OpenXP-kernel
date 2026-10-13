#!/usr/bin/env python3
"""Explicitly create a blank OpenXP-only secondary IDE disk image.
NEVER run this tool against an existing disk. It refuses an existing path.
"""
import argparse
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("path", help="new disk image path (must not exist)")
args = parser.parse_args()
path = Path(args.path)
header = bytearray(512)
header[:8] = b"OXPDISK1"
struct.pack_into("<I", header, 8, 1)
with path.open("xb") as f:
    f.write(header)
    f.truncate(2 * 1024 * 1024)
print(f"Created dedicated OpenXP data image: {path} (2 MiB)")
