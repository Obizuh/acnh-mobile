#!/usr/bin/env python3
"""Regression checks for malformed data and successful-output-only IPA retention."""
import importlib.util,json,os,plistlib,struct,subprocess,sys,tempfile,unittest,zipfile
from pathlib import Path
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('one_click',Path(__file__).with_name('one_click.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class LocalBuildTests(unittest.TestCase):
    def test_data_roundtrip_corruption_and_missing_part(self):
        with tempfile.TemporaryDirectory() as tmp:
            base=Path(tmp);romfs=base/'romfs';(romfs/'Shader').mkdir(parents=True)
            (romfs/m.MARKER).write_bytes(b'synthetic shader marker');(romfs/'text.txt').write_bytes(b'example payload')
            out=base/'pack';subprocess.run([sys.executable,m.ROOT/'tools/pack_data.py',romfs,out],check=True,stdout=subprocess.DEVNULL)
            self.assertEqual(m.verify_data(out),(2,1))
            self.assertTrue(m.data_matches_source(out,romfs,2))
            (romfs/'text.txt').write_bytes(b'wrong source')
            self.assertFalse(m.data_matches_source(out,romfs,2))
            part=out/'ACNH-001.data';data=part.read_bytes();part.write_bytes(bytes([data[0]^1])+data[1:])
            with self.assertRaises(ValueError):m.verify_data(out)
            part.write_bytes(data);part.unlink()
            with self.assertRaises(OSError):m.verify_data(out)
    def test_invalid_index_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            base=Path(tmp);romfs=base/'romfs';(romfs/'Shader').mkdir(parents=True);(romfs/m.MARKER).write_bytes(b'marker')
            out=base/'pack';subprocess.run([sys.executable,m.ROOT/'tools/pack_data.py',romfs,out],check=True,stdout=subprocess.DEVNULL)
            p=out/'ACNH-index.data';raw=bytearray(p.read_bytes());raw[0]^=1;p.write_bytes(raw)
            with self.assertRaises(ValueError):m.verify_data(out)
    def test_two_ipas_remove_same_version_duplicates_preserve_previous(self):
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp)
            def package(name,version):
                p=out/name
                with zipfile.ZipFile(p,'w') as z:z.writestr('Payload/App.app/Info.plist',plistlib.dumps(dict(CFBundleIdentifier='org.acnhmobile.experimental',CFBundleShortVersionString=version,CFBundleVersion='1')))
                return p
            old=package('ACNHMobile-old.ipa','0.2.0');previous=package('ACNHMobile-previous.ipa','0.3.0');duplicate=package('ACNHMobile-duplicate.ipa','0.3.1');new=package('ACNHMobile-new.ipa','0.3.1')
            for i,p in enumerate([old,previous,duplicate,new]):os.utime(p,(100+i,100+i))
            removed=[];original=m.trash_file;m.trash_file=lambda p:removed.append(p)
            try:m.retain_two(out,new)
            finally:m.trash_file=original
            self.assertEqual(set(removed),{old,duplicate});self.assertTrue(previous.exists());self.assertTrue(new.exists())
    def test_invalid_new_package_prevents_cleanup(self):
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp);new=out/'ACNHMobile-new.ipa';new.write_bytes(b'not an IPA')
            with self.assertRaises(zipfile.BadZipFile):m.retain_two(out,new)
    def test_fingerprint_rejects_symlink(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'file').write_bytes(b'file');before=m.romfs_fingerprint(root)
            (root/'file').write_bytes(b'changed');self.assertNotEqual(before,m.romfs_fingerprint(root))
            (root/'link').symlink_to(root/'file')
            with self.assertRaises(ValueError):m.romfs_fingerprint(root)
if __name__=='__main__':unittest.main()
