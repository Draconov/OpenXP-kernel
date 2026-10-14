"""Host tests for the tiny OXRD builder; no compiler needed."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from mkinitrd import elf_from_flat, elf_from_native_flat, pack_archive
from check_windowd_bss import check_map


class BuilderTests(unittest.TestCase):
    def test_elf_template(self):
        elf = elf_from_flat(b"\x0f\x0b")
        self.assertEqual(elf[:4], b"\x7fELF")
        self.assertEqual(int.from_bytes(elf[16:18], "little"), 3)
        self.assertEqual(int.from_bytes(elf[24:32], "little"), 0x100000)
        self.assertEqual(elf[120:], b"\x0f\x0b")

    def test_native_wx_segments(self):
        image = elf_from_native_flat(bytes(6500))
        self.assertEqual(int.from_bytes(image[56:58],"little"),2)
        self.assertEqual(int.from_bytes(image[68:72],"little"),5)
        self.assertEqual(int.from_bytes(image[124:128],"little"),6)
        self.assertEqual(int.from_bytes(image[136:144],"little"),0x102000)
        self.assertEqual(len(image),176+6500)

    def test_large_window_service_data_segment(self):
        image = elf_from_native_flat(bytes(5000), 512 * 1024)
        self.assertEqual(int.from_bytes(image[160:168], "little"), 512 * 1024)
        self.assertEqual(check_map("104000 104000 59270 16 .bss\n"), 0x59270)
        with self.assertRaises(ValueError):
            check_map("104000 104000 80001 16 .bss\n")

    def test_archive_sort_order(self):
        blob = pack_archive([("bin/z", b"ZZ"), ("bin/a", b"AA")])
        self.assertEqual(blob[:4], b"OXRD")
        self.assertEqual(int.from_bytes(blob[12:16], "little"), len(blob))
        self.assertEqual(blob[16:21], b"bin/a")
        self.assertEqual(blob[-4:], b"AAZZ")

    def test_invalid_inputs(self):
        for path in ("/absolute", "bin/../bad", "bin//bad", "bin/./bad", "bin/bad ", "a" * 49):
            with self.subTest(path=path):
                with self.assertRaises(ValueError):
                    pack_archive([(path, b"x")])
        with self.assertRaises(ValueError):
            pack_archive([("a", b"x"), ("a", b"z")])
        with self.assertRaises(ValueError):
            elf_from_flat(b"")
        with self.assertRaises(ValueError):
            elf_from_flat(bytes(4097))


if __name__ == "__main__":
    unittest.main()
