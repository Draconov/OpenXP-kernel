"""Aurora 6: prevent image-backed native apps outgrowing their Ring 3 RW maps.

Past releases reached the GUI before crashing because a kernel app loader
reserved less writable memory than a native C object's static sprite cache.
This check reads the *built* ELF object sections and OXRD boot images; it is
not an estimate derived from source string matching.
"""
import pathlib
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
THEMED = ('run', 'settings', 'filemanager', 'wordpad', 'calculator', 'notepad',
          'calendar', 'paint', 'terminal', 'clock', 'monitor', 'about')


def bss_bytes(path):
    data = path.read_bytes()
    assert data[:5] == b'\x7fELF\x02'
    section_off = struct.unpack_from('<Q', data, 40)[0]
    header_size, count = struct.unpack_from('<HH', data, 58)
    assert header_size >= 64
    total = 0
    for i in range(count):
        section = struct.unpack_from('<IIQQQQIIQQ', data, section_off + i * header_size)
        kind, flags, size = section[1], section[2], section[5]
        if kind == 8 and flags & 2:  # SHT_NOBITS | SHF_ALLOC
            total += size
    return total


def boot_apps():
    content = (ROOT / 'build/initrd.img').read_bytes()
    magic, version, count, length = struct.unpack_from('<4sIII', content)
    assert (magic, version, length) == (b'OXRD', 1, len(content))
    result = {}
    for i in range(count):
        path, off, size, _, _ = struct.unpack_from('<48sIIII', content, 16 + i * 64)
        name = path.split(b'\0', 1)[0].decode('ascii')
        if name.startswith('bin/'):
            result[name[4:]] = content[off:off+size]
    return result


def writable_bytes(elf):
    assert elf[:5] == b'\x7fELF\x02'
    phoff = struct.unpack_from('<Q', elf, 32)[0]
    phsize, phcount = struct.unpack_from('<HH', elf, 54)
    writable = 0
    for i in range(phcount):
        ph = struct.unpack_from('<IIQQQQQQ', elf, phoff + i * phsize)
        if ph[0] == 1 and ph[1] & 2:  # PT_LOAD and PF_W
            writable += ph[6]
    return writable


class NativeImageBudgets(unittest.TestCase):
    def test_all_themed_apps_have_room_for_all_three_atlases(self):
        apps = boot_apps()
        for app in THEMED:
            with self.subTest(app=app):
                self.assertIn(app, apps)
                obj = ROOT / 'build/apps' / (app + '.native.o')
                self.assertTrue(obj.is_file())
                bss = bss_bytes(obj)
                allocated = writable_bytes(apps[app])
                self.assertGreaterEqual(bss, 200000, 'Expected bitmap control, chrome, toolbar, font and icon caches')
                self.assertGreaterEqual(allocated-bss, 4096, 'No writable headroom')
                self.assertLessEqual(allocated, 256 * 1024, 'Exceeded intended app budget')


if __name__ == '__main__':
    unittest.main()
