"""Prevent empty or accidentally omitted PNG masters in Aurora releases."""
import pathlib
import unittest
from tools.pack_skin import png_rgba, build

ROOT=pathlib.Path(__file__).resolve().parents[1]
ART=ROOT/'assets/ui/aurora'
class ArtworkAssetsTest(unittest.TestCase):
    def test_png_masters_exist_with_expected_sizes(self):
        expected={
            'frame-active.png':(64,64),'frame-inactive.png':(64,64),
            'caption-buttons.png':(72,72),'taskbar.png':(48,40),
            'start.png':(120,40),'widgets-atlas.png':(192,128),
            'icons-32.png':(192,64),'chrome-strip.png':(192,64),
            'menu-atlas.png':(192,128),'toolbar-atlas.png':(192,64),
        }
        for filename, dimensions in expected.items():
            with self.subTest(filename=filename):
                path=ART/filename
                self.assertTrue(path.is_file(),f'Missing artist asset: {path}')
                width,height,rgba=png_rgba(path)
                self.assertEqual((width,height),dimensions)
                self.assertGreater(len(set(rgba[i:i+4] for i in range(0,len(rgba),4))),8)
        self.assertTrue((ART/'AURORA-ARTWORK-PREVIEW.png').is_file())
    def test_active_inactive_and_states_are_not_identical(self):
        self.assertNotEqual((ART/'frame-active.png').read_bytes(),
                            (ART/'frame-inactive.png').read_bytes())
        _,_,rgba=png_rgba(ART/'caption-buttons.png')
        w=72
        def pixel(x,y):return rgba[(y*w+x)*4:(y*w+x+1)*4]
        self.assertNotEqual(pixel(8,9),pixel(32,9))
        self.assertNotEqual(pixel(8,56),pixel(8,9))
    def test_packer_accepts_master_assets(self):
        encoded=build(ART)
        self.assertTrue(encoded.startswith(b'OXSK'))
        self.assertLessEqual(len(encoded),65536)

if __name__=='__main__': unittest.main()
