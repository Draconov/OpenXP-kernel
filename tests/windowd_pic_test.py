#!/usr/bin/env python3
"""PIC regression guard: the bad 0x104450 absolute references must be rejected."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from check_windowd_pic import unsafe_relocations


class WindowdPicTests(unittest.TestCase):
    def test_reject_absolute_relocations(self):
        for kind in ("R_X86_64_32", "R_X86_64_32S", "R_X86_64_64"):
            text = f"000000000100  000200000001 {kind:21s} 0000000000000000 .rodata + 0"
            self.assertEqual(len(unsafe_relocations(text)), 1)

    def test_allow_only_pc_relative(self):
        text = "\n".join(f"000000000100  000200000001 {kind:21s} 0000000000000000 .rodata - 4"
                         for kind in ("R_X86_64_PC32", "R_X86_64_PLT32", "R_X86_64_PC64"))
        self.assertEqual(unsafe_relocations(text), [])

    def test_empty_relocations_are_valid(self):
        self.assertEqual(unsafe_relocations("There are no relocations in this file."), [])


if __name__ == "__main__":
    unittest.main()
