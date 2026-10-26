#!/usr/bin/env python3
"""Regression checks for malformed data and successful-output-only IPA retention."""
import importlib.util,json,os,plistlib,struct,subprocess,sys,tempfile,unittest,zipfile
from pathlib import Path
from unittest.mock import patch
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('one_click',Path(__file__).with_name('one_click.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class LocalBuildTests(unittest.TestCase):
    def test_mac_launcher_packages_app_and_cleans_only_successful_job(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);out=root/'outputs';work=root/'work';prepared=root/'prepared';prepared.mkdir();(prepared/'keep').write_text('cache')
            config=root/'config.json';config.write_text(json.dumps(dict(data_root=str(prepared),output_root=str(out),work_root=str(work))))
            version=json.loads((m.ROOT/'config/version.json').read_text());calls=[]
            data=dict(build_root=m.ROOT,patched=prepared,romfs=prepared,shaders=prepared,font=prepared/'font')
            def run(command,log):
                calls.append(command);app=Path(command[command.index('--output')+1]);stage=Path(command[command.index('--work')+1]);(stage/'logs').mkdir(parents=True);(stage/'logs/build.log').write_text('passed')
                (app/'Contents/MacOS').mkdir(parents=True)
                (app/'Contents/Info.plist').write_bytes(plistlib.dumps(dict(CFBundleIdentifier='org.acnhmobile.experimental',CFBundleShortVersionString=version['version'],CFBundleExecutable='Game')))
                executable=app/'Contents/MacOS/Game';executable.write_bytes(b'synthetic executable');executable.chmod(0o755)
            import build_dependencies
            with patch.object(sys,'argv',['one_click.py','mac-app','--config',str(config)]),patch.object(m.platform,'system',return_value='Darwin'),patch.object(m.platform,'machine',return_value='arm64'),patch.object(build_dependencies,'ensure'),patch.object(m,'inputs',return_value=data),patch.object(m,'run',side_effect=run):m.main()
            self.assertEqual(calls[0][2],'mac-app');self.assertTrue((out/'ACNH Mobile.app').is_dir())
            job=next(p for p in work.iterdir() if p.is_dir())
            self.assertFalse((job/'mac-work').exists());self.assertTrue((job/'retained-logs/mac-work/logs/build.log').exists());self.assertTrue((prepared/'keep').exists())
            self.assertFalse(list(out.glob('*.ipa')))
    def test_mac_app_rejects_wrong_identity_and_missing_executable(self):
        with tempfile.TemporaryDirectory() as tmp:
            app=Path(tmp);(app/'Contents').mkdir();p=app/'Contents/Info.plist';version={'version':'1.0'}
            p.write_bytes(plistlib.dumps(dict(CFBundleIdentifier='other',CFBundleShortVersionString='1.0')))
            with self.assertRaises(ValueError):m.inspect_mac_app(app,version)
            p.write_bytes(plistlib.dumps(dict(CFBundleIdentifier='org.acnhmobile.experimental',CFBundleShortVersionString='1.0',CFBundleExecutable='Game')))
            with self.assertRaises(ValueError):m.inspect_mac_app(app,version)
    def test_interactive_modes_ignore_old_input_paths_and_allow_explicit_overrides(self):
        import build_dependencies,prepare_game,io
        from contextlib import redirect_stdout
        with tempfile.TemporaryDirectory() as tmp:
            config=Path(tmp)/'config.json';config.write_text(json.dumps(dict(data_root='/old/cache',game_dir='/old/game',keys='/old/keys',input_mode='prepared')))
            for mode in ('ipa','ipa-data','mac-app'):
                for explicit in (False,True):
                    args=['one_click.py',mode,'--plan','--select','--config',str(config)]
                    if explicit:args+=['--game-dir','/chosen/game','--keys','/chosen/prod.keys','--output-dir','/chosen/output']
                    with patch.object(sys,'argv',args),patch.object(m.platform,'system',return_value='Darwin'),patch.object(m.platform,'machine',return_value='arm64'),patch.object(build_dependencies,'ensure',return_value={}),patch.object(prepare_game,'picker',side_effect=[Path('/chosen/game'),Path('/chosen/prod.keys'),Path('/chosen/output')]) as picker,patch.object(prepare_game,'raw_plan') as validate,patch.object(m,'inputs') as prepared,redirect_stdout(io.StringIO()):
                        m.main()
                    validate.assert_called_once_with(Path('/chosen/game'),Path('/chosen/prod.keys'));prepared.assert_not_called()
                    self.assertEqual(picker.call_count,0 if explicit else 3)
    def test_pickers_run_before_dependency_install_and_survive_restart(self):
        import build_dependencies,prepare_game,io
        from contextlib import redirect_stdout
        with tempfile.TemporaryDirectory() as tmp:
            config=Path(tmp)/'config.json';config.write_text('{}');events=[]
            def pick(kind):events.append(kind);return Path('/picked/'+kind)
            def ensure(**kwargs):
                events.append('dependencies')
                self.assertEqual(events,['game','keys','output','dependencies'])
                self.assertEqual(sys.argv[-6:],['--game-dir','/picked/game','--keys','/picked/keys','--output-dir','/picked/output'])
                raise RuntimeError('stop before build')
            with patch.object(sys,'argv',['one_click.py','ipa','--select','--config',str(config)]),patch.object(m.platform,'system',return_value='Darwin'),patch.object(m.platform,'machine',return_value='arm64'),patch.object(prepare_game,'picker',side_effect=pick),patch.object(build_dependencies,'ensure',side_effect=ensure),redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(RuntimeError,'stop before build'):m.main()
    def test_success_cleanup_preserves_outputs_inputs_logs_and_other_jobs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);work=root/'work';job=work/'current';job.mkdir(parents=True);output=root/'output';output.mkdir();(output/'complete.ipa').write_text('verified')
            cache=work/'prepared/hash';(cache/'source').mkdir(parents=True);(cache/'logs').mkdir();(cache/'logs/prepare.log').write_text('log');(cache/'derived').write_text('cache')
            stage=job/'build-work';stage.mkdir();(stage/'build.log').write_text('log');(job/'run.log').write_text('log')
            other=work/'other-job';other.mkdir();original=root/'original.nsp';original.write_text('owned')
            m.cleanup_success(work,job,output,dict(build_root=cache/'source'),True,[original])
            self.assertFalse(cache.exists());self.assertFalse(stage.exists());self.assertTrue(other.exists());self.assertTrue(original.exists());self.assertTrue((output/'complete.ipa').exists());self.assertTrue((job/'run.log').exists());self.assertTrue((job/'retained-logs/hash/logs/prepare.log').exists())
    def test_cleanup_skips_output_inside_cache_and_symlink_escape(self):
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp)/'work';job=work/'job';job.mkdir(parents=True);cache=work/'prepared/hash';(cache/'source').mkdir(parents=True);out=cache/'output';out.mkdir()
            outside=Path(tmp)/'outside';outside.mkdir();(outside/'keep').write_text('owned');(job/'build-work').symlink_to(outside,target_is_directory=True)
            m.cleanup_success(work,job,out,dict(build_root=cache/'source'),True)
            self.assertTrue(cache.exists());self.assertTrue((outside/'keep').exists())
    def test_cross_device_data_publish_verifies_before_final_rename(self):
        import errno
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);romfs=root/'romfs';(romfs/'Shader').mkdir(parents=True);(romfs/m.MARKER).write_bytes(b'marker')
            source=root/'source';target=root/'output';subprocess.run([sys.executable,m.ROOT/'tools/pack_data.py',romfs,source],check=True,stdout=subprocess.DEVNULL)
            rename=Path.rename
            def cross_device(path,dest):
                if path==source:raise OSError(errno.EXDEV,'cross device')
                return rename(path,dest)
            with patch.object(Path,'rename',cross_device):m.publish_data(source,target)
            self.assertEqual(m.verify_data(target),(1,1));self.assertFalse(source.exists())
    def test_failed_cross_device_copy_preserves_source_and_existing_ipa(self):
        import errno
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'source';source.mkdir();(source/'ACNH-index.data').write_bytes(b'index');target=root/'target'
            def bad_copy(src,dst):
                dst.mkdir();(dst/'ACNH-index.data').write_bytes(b'broken')
            with patch.object(Path,'rename',side_effect=OSError(errno.EXDEV,'cross device')),patch.object(m.shutil,'copytree',side_effect=bad_copy):
                with self.assertRaises(ValueError):m.publish_data(source,target)
            self.assertTrue(source.exists());self.assertFalse(target.exists())
            ipa=root/'source.ipa';ipa.write_bytes(b'new');dest=root/'old.ipa';dest.write_bytes(b'old')
            with self.assertRaises(ValueError):m.publish_ipa(ipa,dest,'wrong digest')
            self.assertEqual(dest.read_bytes(),b'old');self.assertEqual(ipa.read_bytes(),b'new')
            m.publish_ipa(ipa,root/'new.ipa',m.sha(ipa));self.assertFalse(ipa.exists());self.assertEqual((root/'new.ipa').read_bytes(),b'new')
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
