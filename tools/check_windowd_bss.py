#!/usr/bin/env python3
"""Fail the build if windowd's linked .bss outgrows its private RW ELF segment."""
from pathlib import Path
import re
import sys

WINDOWD_RW_BYTES = 512 * 1024

def check_map(contents: str) -> int:
    matches = re.findall(r"^\s*[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+\d+\s+\.bss\s*$", contents, re.M)
    if len(matches) != 1:
        raise ValueError("cannot find linked .bss section in linker map")
    size = int(matches[0], 16)
    if size > WINDOWD_RW_BYTES:
        raise ValueError(f"windowd .bss {size} bytes exceeds {WINDOWD_RW_BYTES}-byte RW segment")
    return size

if __name__ == "__main__":
    try:
        print(f"windowd RW memory: {check_map(Path(sys.argv[1]).read_text())} / {WINDOWD_RW_BYTES} bytes")
    except (ValueError, OSError, IndexError) as exc:
        sys.exit(f"ERROR: windowd RW memory validation failed: {exc}")
