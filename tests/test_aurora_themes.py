"""Aurora 8: all nineteen theme + font resources are in the boot archive."""
import pathlib,struct,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from pack_skin import build as skin_build,png_rgba
from pack_ui_atlases import encode
NAMES=('aurora','olive','midnight')
ATLAS=(('widgets-atlas.png','widgets',192,128),('icons-32.png','icons',192,64),('chrome-strip.png','chrome',192,64),('menu-atlas.png','menu',192,128),('toolbar-atlas.png','toolbar',192,64))
class ThemePacks(unittest.TestCase):
    def root(self,name):
        return ROOT/'assets/ui/aurora'/('' if name=='aurora' else name)
    def test_all_skins_are_buildable_and_valid(self):
        for name in NAMES:
            art=self.root(name)
            packed=skin_build(art)
            self.assertEqual(packed,(ROOT/'build/skin'/f'{name}.oxs').read_bytes())
            self.assertEqual(packed[:4],b'OXSK')
            self.assertEqual(struct.unpack_from('<H',packed,6)[0],5)
            self.assertLessEqual(len(packed),65536)
    def test_all_widget_and_icon_packs_match_editable_sources(self):
        for name in NAMES:
            for file,kind,w,h in ATLAS:
                encoded=encode(self.root(name)/file,w,h)
                self.assertEqual(encoded,(ROOT/'build/skin'/f'{name}-{kind}.oxua').read_bytes())
                self.assertEqual(len(encoded),16+w*h*3)
    def test_all_nineteen_theme_resources_ship_in_boot_archive(self):
        content=(ROOT/'build/initrd.img').read_bytes()
        magic,version,count,total=struct.unpack_from('<4sIII',content)
        self.assertEqual((magic,version,total),(b'OXRD',1,len(content)))
        embedded={}
        for n in range(count):
            entry=struct.unpack_from('<48sIIII',content,16+64*n)
            path=entry[0].split(b'\0',1)[0].decode('ascii')
            off,size=entry[1:3]
            self.assertLessEqual(off+size,len(content))
            embedded[path]=content[off:off+size]
        self.assertEqual(embedded.get('windows/resources/aurora-font.oxft'),
                         (ROOT/'build/skin/aurora-font.oxft').read_bytes())
        for theme in NAMES:
            for suffix in ('.oxs','-widgets.oxua','-icons.oxua','-chrome.oxua','-menu.oxua','-toolbar.oxua'):
                name=theme+suffix
                self.assertEqual(embedded.get('windows/resources/'+name),
                                 (ROOT/'build/skin'/name).read_bytes())
    def test_three_distinct_appearances_and_png_dimensions(self):
        for file in ('frame-active.png','frame-inactive.png','caption-buttons.png','taskbar.png','start.png','widgets-atlas.png','icons-32.png','chrome-strip.png','menu-atlas.png','toolbar-atlas.png'):
            pixels=[png_rgba(self.root(name)/file) for name in NAMES]
            self.assertEqual(len({(w,h) for w,h,_ in pixels}),1,file)
            self.assertEqual(len({raw for _,_,raw in pixels}),3,file)
    def test_theme_switch_paths_match_initrd(self):
        # String constants must match the actual file names bundled in mkinitrd.
        routes=(ROOT/'apps/native/theme.h').read_text()
        for name in NAMES:
            for suffix in ('.oxs','-widgets.oxua','-icons.oxua','-chrome.oxua','-menu.oxua','-toolbar.oxua'):
                self.assertIn('windows/resources/'+name+suffix,routes)
if __name__=='__main__':unittest.main()
