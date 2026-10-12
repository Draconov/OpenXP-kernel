#!/usr/bin/env python3
"""Generate an OXRD v1 archive containing small position-independent ELF64 apps.

Input files must be freestanding flat x86-64 code compiled without relocations.
The generated ELF files use ET_DYN (relative virtual addresses) and a single
read+execute PT_LOAD; they do not have a dynamic interpreter or relocations.
"""
import argparse
import pathlib
import struct
import zlib

BASE = 0x100000
MAX_IMAGE = 1024 * 1024

def elf_from_flat(code: bytes) -> bytes:
    if not 0 < len(code) <= 4096:
        raise ValueError("app must have 1..4096 bytes of PIC code")
    ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + bytes(8)
    header = struct.pack("<16sHHIQQQIHHHHHH", ident, 3, 62, 1,
                         BASE, 64, 0, 0, 64, 56, 1, 0, 0, 0)
    phdr = struct.pack("<IIQQQQQQ", 1, 5, 120, BASE, 0,
                       len(code), len(code), 1)
    return header + phdr + code

def pack_archive(entries: list[tuple[str, bytes]]) -> bytes:
    if not 1 <= len(entries) <= 16:
        raise ValueError("archive needs 1..16 files")
    table = bytearray()
    payload = bytearray()
    for name, content in sorted(entries):
        parts = name.split("/")
        if not all(part and part not in (".", "..") and all(
                c.isascii() and (c.isalnum() or c in "_-." ) for c in part)
                   for part in parts):
            raise ValueError(f"invalid file name: {name!r}")
        encoded = name.encode("ascii")
        if len(encoded) >= 48 or not content:
            raise ValueError("name too long or empty file")
        offset = 16 + 64 * len(entries) + len(payload)
        table.extend(struct.pack("<48sIIII", encoded, offset, len(content),
                                 zlib.crc32(content), 1))
        payload.extend(content)
    if len(set(name for name, _ in entries)) != len(entries):
        raise ValueError("duplicate name")
    total = 16 + len(table) + len(payload)
    if total > MAX_IMAGE:
        raise ValueError("archive too large")
    return struct.pack("<4sIII", b"OXRD", 1, len(entries), total) + table + payload

def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("output", type=pathlib.Path)
    p.add_argument("hello", type=pathlib.Path)
    p.add_argument("fault", type=pathlib.Path)
    args = p.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    entries = [("bin/hello", elf_from_flat(args.hello.read_bytes())),
               ("bin/fault", elf_from_flat(args.fault.read_bytes())),
               ("docs/readme.txt", b"OpenXP 1.8.0 - Experimental desktop\n"
                b"This RAM drive is read-only.\n"
                b"Launch programs with RUN BIN/HELLO.\n"
                b"Mouse and keyboard use legacy PS/2.\n")]
    args.output.write_bytes(pack_archive(entries))

if __name__ == "__main__":
    main()
