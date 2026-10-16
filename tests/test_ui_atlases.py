import pathlib,struct,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from pack_ui_atlases import SOURCES,encode
class AtlasTests(unittest.TestCase):
    def test_exact_format_and_png_content(self):
        for name,w,h,out in SOURCES:
            payload=encode(ROOT/'assets/ui/aurora'/name,w,h)
            self.assertEqual(payload[:4],b'OXUA')
            self.assertEqual(struct.unpack_from('<HHHHI',payload,4),(1,w,h,0,w*h*3))
            self.assertEqual(len(payload),16+w*h*3)
            self.assertEqual(payload,(ROOT/'build/skin'/out).read_bytes())
    def test_pixel_art_is_not_blank(self):
        for name,w,h,out in SOURCES:
            data=encode(ROOT/'assets/ui/aurora'/name,w,h)[16:]
            self.assertGreater(len(set(data[2::3])),1)  # transparency mix
            self.assertGreater(len(set(data[0::3])),5)  # real artwork
    def test_all_twelve_icon_tiles_contain_artwork(self):
        data=encode(ROOT/'assets/ui/aurora/icons-32.png',192,64)[16:]
        for icon in range(12):
            left=(icon%6)*32
            top=(icon//6)*32
            opaque=0
            for yy in range(top,top+32):
                for xx in range(left,left+32):
                    if data[3*(yy*192+xx)+2]>0:opaque+=1
            self.assertGreater(opaque,30,f'Icon #{icon} has no visible artwork')
    def test_reject_misdimensioned_art(self):
        name,w,h,_=SOURCES[0]
        with self.assertRaises(ValueError):encode(ROOT/'assets/ui/aurora'/name,w-1,h)
if __name__=='__main__':unittest.main()
