#!/usr/bin/env python3
"""PIC regression guard: the bad 0x104450 absolute references must be rejected."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from check_windowd_pic import unsafe_relocations
from check_windowd_bss import check_map


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


    def test_reject_initialized_input_data_hidden_in_rx_text(self):
        # lld's map folds .data into the .text output section. Looking only
        # for the (absent) standalone output .data missed the Aurora 17 crash.
        link_map = """          100000           100000      7d70    16 .text
          107d68           107d68         8     4 build/apps/windowd.native.o:(.data)
          107d68           107d68         4     1                 task_hover
          107d6c           107d6c         4     1                 shell_press_target
          108000           108000    191b28    16 .bss
"""
        with self.assertRaisesRegex(ValueError, "initialized writable input section"):
            check_map(link_map)

    def test_accept_zero_initialized_globals(self):
        link_map = """          100000           100000      7d68    16 .text
          107d68           107d68         0     4 build/apps/windowd.native.o:(.data)
          108000           108000    191b28    16 .bss
          108000           108000        16     4 build/apps/windowd.native.o:(.bss)
"""
        self.assertEqual(check_map(link_map), 0x191b28)

    def test_reject_data_sections_other_than_plain_data(self):
        for section in (".data.x", ".sdata", ".tdata", ".init_array", ".got"):
            with self.subTest(section=section):
                link_map = ("  100000  100000  8  1 build/apps/test.o:(" + section + ")\n"
                            "  108000  108000  100  16 .bss\n")
                with self.assertRaises(ValueError):
                    check_map(link_map)


if __name__ == "__main__":
    unittest.main()
