#!/usr/bin/env python3
"""Reject unsupported floating-point or SIMD instructions in Ring 3 objects.

OpenXP does not yet enable/preserve SSE/XMM or FPU state for user processes.
Checking disassembled *objects* rather than raw native .bin images avoids
false positives from data bytes and is independent of linker layout.
"""
from pathlib import Path
import re
import subprocess
import sys

# With 'objdump -d -M intel', disassembly lines have address, bytes, mnemonic
# and operands separated by tabs. Only examine instructions, not data/headers.
# GNU objdump inserts tabs before *both* the bytes and mnemonic. Requiring
# those separators is important: multi-line hex immediates have no mnemonic.
ADDRESS = re.compile(r"^\s*[0-9a-f]+:$", re.I)
BYTES = re.compile(r"^\s*(?:[0-9a-f]{2}\s*)+$", re.I)
VECTOR_REGISTER = re.compile(r"\b(?:xmm|ymm|zmm|mm)[0-9]+\b|\bst(?:\([0-7]\))?\b", re.I)
VECTOR_MEMORY = re.compile(r"\b(?:XMMWORD|YMMWORD|ZMMWORD|TBYTE)\s+PTR\b", re.I)
# Instructions that may appear without XMM/x87 operands.
SPECIAL = re.compile(r"^(?:emms|femms|finit|fninit|fwait|wait|fxsave(?:64)?|fxrstor(?:64)?|xsave(?:opt|c|s|64)?|xrstor(?:s|64)?|vzeroall|vzeroupper|ldmxcsr|stmxcsr)$", re.I)
FPU_PREFIX = re.compile(r"^f", re.I)  # all x87 opcodes (including fnst*, fyl2x, etc.)


def forbidden_instruction(line: str) -> str | None:
    parts = line.split("\t", 2)
    if len(parts) != 3 or not ADDRESS.match(parts[0]) or not BYTES.match(parts[1]):
        return None
    instruction = parts[2].strip().split(None, 1)
    if not instruction:
        return None
    mnemonic = instruction[0]
    operands = instruction[1] if len(instruction) == 2 else ""
    if VECTOR_REGISTER.search(operands) or VECTOR_MEMORY.search(operands) or SPECIAL.fullmatch(mnemonic) or FPU_PREFIX.match(mnemonic):
        return mnemonic
    return None


def check_file(path: Path) -> list[str]:
    result = subprocess.run(["objdump", "-d", "-M", "intel", str(path)],
                            capture_output=True, text=True, check=True)
    failures = []
    for line in result.stdout.splitlines():
        bad = forbidden_instruction(line)
        if bad:
            failures.append(f"{path}: {line.strip()}")
    return failures


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: check_ring3_isa.py BUILD/apps", file=sys.stderr)
        return 2
    directory = Path(sys.argv[1])
    objects = sorted(directory.glob("*.o"))
    expected = {"shell_raster.o", "native_runtime.o", "windowd.native.o", "native_scene.o", "wm_protocol.o", "wallpaper.o"}
    # Audit every native app, not just the shared libraries: Make considers
    # pattern-rule outputs intermediate unless explicitly preserved.
    app_names = {p.stem for p in directory.glob("*.bin")} - {"hello", "fault"}
    expected |= {f"{name}.native.o" for name in app_names}
    expected |= {"native_start.o", "hello.o", "fault.o"}
    missing = expected - {p.name for p in objects}
    if missing:
        print(f"ERROR: Ring 3 ISA check incomplete; missing {', '.join(sorted(missing))}", file=sys.stderr)
        return 1
    try:
        failures = [line for obj in objects for line in check_file(obj)]
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: Ring 3 disassembly failed: {exc}", file=sys.stderr)
        return 1
    if failures:
        print("ERROR: Ring 3 uses unsupported SIMD/x87 instructions (no task state save):", file=sys.stderr)
        for line in failures[:30]:
            print("  " + line, file=sys.stderr)
        if len(failures) > 30:
            print(f"  ... and {len(failures) - 30} more", file=sys.stderr)
        return 1
    print(f"PASS: {len(objects)} Ring 3 object files contain no disallowed SIMD/x87 instructions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
