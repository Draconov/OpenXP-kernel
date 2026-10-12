"""Host tests for the tiny OXRD builder; no compiler needed."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from mkinitrd import elf_from_flat, pack_archive


class BuilderTests(unittest.TestCase):
    def test_elf_template(self):
        elf = elf_from_flat(b"\x0f\x0b")
        self.assertEqual(elf[:4], b"\x7fELF")
        self.assertEqual(int.from_bytes(elf[16:18], "little"), 3)
        self.assertEqual(int.from_bytes(elf[24:32], "little"), 0x100000)
        self.assertEqual(elf[120:], b"\x0f\x0b")

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
