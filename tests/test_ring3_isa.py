"""Regression test: disassembler must detect the exact #UD fault in CI logs."""
from pathlib import Path
import importlib.util
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ring3_isa', ROOT / 'tools' / 'check_ring3_isa.py')
isa = importlib.util.module_from_spec(spec)
spec.loader.exec_module(isa)


class Ring3ISATest(unittest.TestCase):
    def test_detect_ci_failure(self):
        self.assertEqual(isa.forbidden_instruction('  54:\t66 0f 6e c0\tmovd   xmm0,eax'), 'movd')
        self.assertEqual(isa.forbidden_instruction('  59:\tf2 0f 70 c0 00\tpshuflw xmm0,xmm0,0x0'), 'pshuflw')
        self.assertEqual(isa.forbidden_instruction('  70:\tf3 0f 7f 04 40\tmovdqu XMMWORD PTR [rax],xmm0'), 'movdqu')

    def test_detect_state_and_x87(self):
        self.assertEqual(isa.forbidden_instruction('  0:\t0f 77\temms'), 'emms')
        self.assertEqual(isa.forbidden_instruction('  0:\td9 e8\tfld1'), 'fld1')
        self.assertEqual(isa.forbidden_instruction('  0:\t0f ae 04 24\tfxsave [rsp]'), 'fxsave')

    def test_allow_integer_instructions(self):
        self.assertIsNone(isa.forbidden_instruction('  a:\t66 0f 1f 44 00 00\tnop    WORD PTR [rax+rax*1+0x0]'))
        self.assertIsNone(isa.forbidden_instruction('  10:\t48 83 ec 40\tsub rsp,0x40'))
        self.assertIsNone(isa.forbidden_instruction('  0:\t48 b8 00 00 00 00 00 00 00 00\tmovabs rax,0'))
        self.assertIsNone(isa.forbidden_instruction('0000000000000000 <oxp_raster_row>:'))


if __name__ == '__main__':
    unittest.main()
