"""Synthetic NRO assembly packing: shared sections, alignment, refusal-only manifests."""
from pathlib import Path
import subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class NroEmbed(unittest.TestCase):
 def test_packed_sections(self):
  with tempfile.TemporaryDirectory() as d:
   d=Path(d)
   for n in ['main','nro0','nro1']:
    (d/(n+'.patched.bin')).write_bytes(bytes(0xc000))
    (d/(n+'.fixups.tsv')).write_text('# TEXT_END 4000\n# PATCH_OFF 8000\n# KEY_PAGE_OFF 4000\n# KEY_PAGE_SIZE 4000\n')
   (d/'nro.tsv').write_text('# name\n'+'nro0\tx\n'+'refused\ty\n'+'nro1\tz\n')
   out=d/'embed.S';r=subprocess.run([sys.executable,str(ROOT/'dm/ios/tools/gen_embed.py'),str(d),str(out),'main'],capture_output=True,text=True,check=True)
   s=out.read_text();self.assertEqual(s.count('.section __TEXT,__dm_nro_x0'),1);self.assertEqual(s.count('.section __DM_NRO_W,__img'),1)
   self.assertIn('.ascii "nro0"',s);self.assertIn('.ascii "nro1"',s);self.assertNotIn('.ascii "refused"',s)
   self.assertIn('.quad 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000',s)
   self.assertIn('-Wl,-segprot,__DM_NRO_W,rw,rw',r.stdout)
   (d/'nro.tsv').write_text('refused\tx\n');subprocess.run([sys.executable,str(ROOT/'dm/ios/tools/gen_embed.py'),str(d),str(out),'main'],check=True,capture_output=True)
   self.assertNotIn('__dm_nro_tab',out.read_text())
if __name__=='__main__':unittest.main()
