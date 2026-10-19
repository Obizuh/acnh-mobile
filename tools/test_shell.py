#!/usr/bin/env python3
"""Synthetic shell tests: no game files or Apple tools required."""
import importlib.util, json, struct, sys, tempfile, unittest, zipfile
from pathlib import Path
sys.dont_write_bytecode=True
from pack_shell import extract_shell
from build_shell import ROOT, source_id

class ShellTests(unittest.TestCase):
    def test_archive_paths(self):
        for entry in ['../escape', '/absolute', 'Payload/ACNHMobile.app/../../escape', 'Payload/ACNHMobile.app/dir\\escape']:
            with self.subTest(entry=entry), tempfile.TemporaryDirectory() as tmp:
                tmp=Path(tmp)
                with zipfile.ZipFile(tmp/'bad.zip','w') as z:z.writestr(entry,b'x')
                with self.assertRaises(ValueError):extract_shell(tmp/'bad.zip',tmp/'out')

    def test_duplicate_and_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp=Path(tmp)
            entry=zipfile.ZipInfo('Payload/ACNHMobile.app/link');entry.create_system=3;entry.external_attr=0o120777<<16
            with zipfile.ZipFile(tmp/'bad.zip','w') as z:z.writestr(entry,'/outside')
            with self.assertRaises(ValueError):extract_shell(tmp/'bad.zip',tmp/'out')

    def test_valid_shell_extract(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp=Path(tmp)
            with zipfile.ZipFile(tmp/'ok.zip','w') as z:z.writestr('Payload/ACNHMobile.app/dm/shell_info.json','{}')
            extract_shell(tmp/'ok.zip',tmp/'out')
            self.assertEqual(json.loads((tmp/'out/Payload/ACNHMobile.app/dm/shell_info.json').read_text()),{})

    def test_guest_three_modules_byte_layout(self):
        spec=importlib.util.spec_from_file_location('guest',ROOT/'dm/tools/guest_dylib.py');guest=importlib.util.module_from_spec(spec);spec.loader.exec_module(guest)
        with tempfile.TemporaryDirectory() as tmp:
            tmp=Path(tmp); expected={}
            for index,name in enumerate(['main','subsdk0','sdk']):
                # Synthetic bytes, deliberately different for each source page.
                chunks=[bytes([10*index+i+1])*0x4000 for i in range(3)]
                (tmp/(name+'.patched.bin')).write_bytes(b''.join(chunks))
                (tmp/(name+'.fixups.tsv')).write_text('# TEXT_END 4000\n# PATCH_OFF 8000\n')
                expected[('__TEXT','__dm'+name+'x0')]=chunks[0]
                expected[('__DM'+name+'W','__img')]=chunks[1]
                expected[('__TEXT','__dm'+name+'x1')]=chunks[2]
            data=guest.build(str(tmp),['main','subsdk0','sdk'])
            self.assertEqual(data,guest.build(str(tmp),['main','subsdk0','sdk']))
            count=struct.unpack_from('<I',data,16)[0];pos=32;found={}
            for _ in range(count):
                cmd,size=struct.unpack_from('<II',data,pos)
                if cmd==0x19:
                    nsect=struct.unpack_from('<I',data,pos+64)[0]
                    for j in range(nsect):
                        q=pos+72+j*80
                        sn,sg,addr,sz,off=struct.unpack_from('<16s16sQQI',data,q)
                        key=(sg.rstrip(b'\0').decode(),sn.rstrip(b'\0').decode())
                        self.assertEqual(off%0x4000,0);found[key]=data[off:off+sz]
                pos+=size
            self.assertEqual(found,expected)

    def test_source_fingerprint(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp=Path(tmp);p=tmp/'dm/host/a.c';p.parent.mkdir(parents=True);p.write_text('one')
            first=source_id(tmp);p.write_text('two');self.assertNotEqual(first,source_id(tmp))
            first=source_id(tmp);(p.parent/'gen').mkdir();(p.parent/'gen/a.c').write_text('generated')
            self.assertEqual(first,source_id(tmp))

if __name__=='__main__':unittest.main()
