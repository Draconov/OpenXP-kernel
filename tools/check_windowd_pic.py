#!/usr/bin/env python3
"""Ensure that the raw ET_DYN windowd image can run at an arbitrary user base.

The native flat-image packer does not apply ELF dynamic relocations. Objects
used in windowd must therefore have only PC-relative relocations: an absolute
jump table or const-data reference works at 0x100000 but faults at the high
Ring 3 runtime base (0x8000100000 on the current kernel).
"""
from pathlib import Path
import re
import subprocess
import sys

SAFE = frozenset({"R_X86_64_PC32", "R_X86_64_PLT32", "R_X86_64_PC64"})
RELOCATION = re.compile(r"\bR_X86_64_[A-Z0-9_]+\b")


def unsafe_relocations(readelf_output: str) -> list[str]:
    """Return relocation lines that require an absolute runtime address."""
    return [line.strip() for line in readelf_output.splitlines()
            if any(match.group(0) not in SAFE
                   for match in RELOCATION.finditer(line))]


def main(paths: list[str]) -> int:
    if not paths:
        print("Usage: check_windowd_pic.py object.o [...]", file=sys.stderr)
        return 2
    errors = []
    for name in paths:
        if Path(name).suffix != ".o":
            print(f"ERROR: not an ELF object: {name}", file=sys.stderr)
            return 2
        try:
            output = subprocess.run(["readelf", "-rW", name], check=True,
                                    capture_output=True, text=True).stdout
        except (OSError, subprocess.CalledProcessError) as exc:
            print(f"ERROR: cannot inspect {name}: {exc}", file=sys.stderr)
            return 2
        errors.extend(f"{name}: {line}" for line in unsafe_relocations(output))
    if errors:
        print("ERROR: windowd has absolute relocations in a flat ET_DYN image!", file=sys.stderr)
        print("Compile with -fpie and avoid initialized absolute pointer tables.", file=sys.stderr)
        for line in errors[:12]:
            print("  " + line, file=sys.stderr)
        if len(errors) > 12:
            print(f"  ... {len(errors) - 12} more", file=sys.stderr)
        return 1
    print(f"PASS: windowd PIC audit of {len(paths)} objects")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
