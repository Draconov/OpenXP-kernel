#!/usr/bin/env python3
"""Annotate a windowd #UD fault using an ELF-relative RIP from serial output.

Usage: python3 tools/resolve_ring3_fault.py LOG build/apps/windowd.map
This intentionally doesn't infer a fault location from CR2 (which is stale for
invalid-opcode exceptions). A map must come from the same linked build.
"""
import pathlib
import re
import sys

FAULT_RE = re.compile(r"USER EXCEPTION pid=(\d+) vector=(\d+) .*?image_offset=0x([0-9a-fA-F]+)")
SYMBOL_RE = re.compile(r"^\s*([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+\d+\s+([A-Za-z_]\w*)\s*$", re.M)


def resolve(log: str, linker_map: str) -> list[str]:
    symbols = sorted((int(addr, 16), int(size, 16), name)
                     for addr, size, name in SYMBOL_RE.findall(linker_map)
                     if int(size, 16))
    reports = []
    for pid, vector, address in FAULT_RE.findall(log):
        pc = int(address, 16)
        candidates = [(start, size, name) for start, size, name in symbols
                      if start <= pc < start + size]
        if candidates:
            start, _, name = max(candidates, key=lambda entry: entry[0])
            reports.append(f"Ring 3 PID {pid} vector {vector}: windowd!{name}+0x{pc-start:x} "
                           f"(image offset 0x{pc:x})")
        else:
            reports.append(f"Ring 3 PID {pid} vector {vector}: image offset 0x{pc:x} "
                           "(outside known windowd symbols)")
    return reports


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: resolve_ring3_fault.py SERIAL_LOG WINDOWD_MAP", file=sys.stderr)
        return 2
    log = pathlib.Path(sys.argv[1]).read_text(errors="replace")
    linker_map = pathlib.Path(sys.argv[2]).read_text(errors="replace")
    for result in resolve(log, linker_map):
        print(result)
    return 0


if __name__ == "__main__":
    sys.exit(main())
