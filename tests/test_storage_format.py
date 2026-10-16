"""Aurora 17 disk format: explicit new image, backwards-compatible opt-in."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class StorageFormatTests(unittest.TestCase):
    def test_default_v2_and_legacy_flag(self):
        with tempfile.TemporaryDirectory() as d:
            for flag, magic, version in (([], b"OXPDISK2", 2), (["--legacy-v1"], b"OXPDISK1", 1)):
                path = pathlib.Path(d) / f"data{version}.img"
                cmd = [sys.executable, str(ROOT / "tools/mkstorage.py"), *flag, str(path)]
                subprocess.run(cmd, check=True, capture_output=True)
                with path.open("rb") as f:
                    header = f.read(12)
                self.assertEqual(header[:8], magic)
                self.assertEqual(int.from_bytes(header[8:12], "little"), version)
                self.assertEqual(path.stat().st_size, 2 * 1024 * 1024)
                result = subprocess.run(cmd, capture_output=True)
                self.assertNotEqual(result.returncode, 0)

if __name__ == "__main__":
    unittest.main()
