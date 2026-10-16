"""Prevent accidental removal of editable font artwork and generated OXFT."""
from pathlib import Path
import sys, unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from pack_aurora_font import encode
from pack_skin import png_rgba
class FontArtwork(unittest.TestCase):
    def test_source_matches_packed(self):
        source=ROOT/'assets/ui/aurora/fonts/aurora-font.png'
        w,h,rgba=png_rgba(source)
        self.assertEqual((w,h),(176,156))
        self.assertGreater(sum(a>0 for a in rgba[3::4]),300)
        compiled=encode(source)
        self.assertEqual(compiled,(ROOT/'build/skin/aurora-font.oxft').read_bytes())
        self.assertEqual(len(compiled),27662)
    def test_styles_are_not_identical(self):
        _,_,rgba=png_rgba(ROOT/'assets/ui/aurora/fonts/aurora-font.png')
        stride=176*13*6*4
        self.assertNotEqual(rgba[:stride],rgba[stride:2*stride])
if __name__=='__main__':unittest.main()
