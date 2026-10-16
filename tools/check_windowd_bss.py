#!/usr/bin/env python3
"""Validate linked windowd writable state before building the flat native ELF."""
from pathlib import Path
import re
import sys

WINDOWD_RW_BYTES = 2 * 1024 * 1024
# lld's -Map output lists input sections separately from output sections.
# app.ld merges *(.data .data.*) into RX .text, so the OUT .data can be zero
# even when writable initialized globals are present in read-only memory.
# Examine the INPUT sections, including ones folded into RX .text.
INITIALIZED_SECTION = re.compile(
    r"^\s*[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+\d+\s+"
    r"(\S+):\(\.(data(?:\.[\w.-]+)?|sdata(?:\.[\w.-]+)?|"
    r"tdata(?:\.[\w.-]+)?|init_array(?:\.[\w.-]+)?|"
    r"fini_array(?:\.[\w.-]+)?|got(?:\.[\w.-]+)?)\)\s*$",
    re.M,
)

def check_map(contents: str) -> int:
    matches = re.findall(r"^\s*[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+\d+\s+\.bss\s*$", contents, re.M)
    if len(matches) != 1:
        raise ValueError("cannot find linked .bss section in linker map")
    size = int(matches[0], 16)
    # These bytes are emitted inside the image's *read-only* PT_LOAD. Even
    # setting such a global back to its initial value faults in Ring 3.
    initialized = [(obj, section, int(count, 16))
                   for count, obj, section in INITIALIZED_SECTION.findall(contents)
                   if int(count, 16) != 0]
    if initialized:
        detail = ", ".join(f"{obj}:(.{section})={n} bytes"
                           for obj, section, n in initialized[:6])
        raise ValueError("initialized writable input section placed in RX .text: "
                         + detail + "; use zero-initialized globals and "
                         "initialize them in oxp_main")
    # Also reject a separately named output .data if the layout changes.
    data = re.findall(r"^\s*[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+"
                      r"([0-9a-fA-F]+)\s+\d+\s+\.data\s*$", contents, re.M)
    if any(int(count, 16) for count in data):
        raise ValueError("initialized .data output is not supported by the native packer")
    if size > WINDOWD_RW_BYTES:
        raise ValueError(f"windowd .bss {size} bytes exceeds {WINDOWD_RW_BYTES}-byte RW segment")
    return size

if __name__ == "__main__":
    try:
        print(f"windowd RW memory: {check_map(Path(sys.argv[1]).read_text())} / {WINDOWD_RW_BYTES} bytes")
    except (ValueError, OSError, IndexError) as exc:
        sys.exit(f"ERROR: windowd RW memory validation failed: {exc}")
