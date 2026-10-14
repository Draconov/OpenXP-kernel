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
MAX_IMAGE = 4 * 1024 * 1024

def elf_from_flat(code: bytes) -> bytes:
    if not 0 < len(code) <= 4096:
        raise ValueError("app must have 1..4096 bytes of PIC code")
    ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + bytes(8)
    header = struct.pack("<16sHHIQQQIHHHHHH", ident, 3, 62, 1,
                         BASE, 64, 0, 0, 64, 56, 1, 0, 0, 0)
    phdr = struct.pack("<IIQQQQQQ", 1, 5, 120, BASE, 0,
                       len(code), len(code), 1)
    return header + phdr + code

def elf_from_native_flat(code: bytes, writable_size: int = 32768) -> bytes:
    """Relocation-free native image with separate RX code and zero-filled RW data.

    Native linker places .bss on the first page after the flat code. The
    window service needs more private RAM to cache client pixel surfaces;
    other apps retain the small default reservation. This is
    an interim format; future programs should use proper multi-segment ELF.
    """
    if not 0 < len(code) <= 32768:
        raise ValueError("native image must have 1..32768 bytes of PIC code")
    ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + bytes(8)
    header = struct.pack("<16sHHIQQQIHHHHHH", ident, 3, 62, 1,
                         BASE, 64, 0, 0, 64, 56, 2, 0, 0, 0)
    first = struct.pack("<IIQQQQQQ", 1, 5, 176, BASE, 0,
                        len(code), len(code), 1)
    writable = BASE + ((len(code) + 4095) & ~4095)
    second = struct.pack("<IIQQQQQQ", 1, 6, 176 + len(code), writable, 0,
                         0, writable_size, 1)
    return header + first + second + code


def pack_archive(entries: list[tuple[str, bytes]]) -> bytes:
    if not 1 <= len(entries) <= 64:
        raise ValueError("archive needs 1..64 files")
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
    p.add_argument("native", nargs="*", type=pathlib.Path)
    p.add_argument("--wallpaper-dir", type=pathlib.Path, default=pathlib.Path("assets/wallpapers"))
    args = p.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    entries = [("bin/hello", elf_from_flat(args.hello.read_bytes())),
               ("bin/fault", elf_from_flat(args.fault.read_bytes())),
               ("docs/readme.txt", b"OpenXP - Experimental desktop\n"
                b"This RAM drive is read-only.\n"
                b"Launch programs with RUN BIN/HELLO.\n"
                b"Mouse and keyboard use legacy PS/2.\n")]
    for app in args.native:
        entries.append(("bin/" + app.stem, elf_from_native_flat(
            app.read_bytes(), 512 * 1024 if app.stem == "windowd" else 32768)))
    if args.wallpaper_dir.is_dir():
        for image in sorted(args.wallpaper_dir.glob("*.bmp")):
            content = image.read_bytes()
            # Only simple, bounded uncompressed 24-bit BMP assets are accepted.
            if (len(content) < 54 or content[:2] != b"BM" or
                struct.unpack_from("<I", content, 2)[0] != len(content) or
                struct.unpack_from("<I", content, 14)[0] != 40 or
                struct.unpack_from("<H", content, 26)[0] != 1 or
                struct.unpack_from("<H", content, 28)[0] != 24 or
                struct.unpack_from("<I", content, 30)[0] != 0):
                raise ValueError(f"unsupported wallpaper (need uncompressed 24-bit BMP): {image}")
            w, h = struct.unpack_from("<ii", content, 18)
            offset = struct.unpack_from("<I", content, 10)[0]
            if not 1 <= w <= 1024 or not 1 <= h <= 768 or offset < 54 or \
               offset + (((w * 3 + 3) // 4) * 4) * h != len(content):
                raise ValueError(f"bad wallpaper dimensions/data: {image}")
            entries.append((f"windows/web/wallpaper/{image.name}", content))
    args.output.write_bytes(pack_archive(entries))

if __name__ == "__main__":
    main()
