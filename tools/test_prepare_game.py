#!/usr/bin/env python3
"""Check raw-input validation and dependency planning without keys or downloads."""
import os,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).parent))
import prepare_game as p,build_dependencies as d
class PrepareTests(unittest.TestCase):
    def test_selection_and_exact_version(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);keys=root/'prod.keys';keys.write_text('synthetic test placeholder')
            base=root/'[01006F8002326000].nsp';base.write_bytes(b'not a real game')
            update=root/'[01006F8002326800][v2228224].nsp';update.write_bytes(b'not a real game')
            self.assertEqual(p.raw_plan(root,root)[3]['base'],base.resolve())
            update.rename(root/'[01006F8002326800][v1].nsp')
            with self.assertRaisesRegex(ValueError,'3.0.3'):p.raw_plan(root,keys)
    def test_missing_keys_and_wrong_title(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)
            with self.assertRaisesRegex(ValueError,'prod.keys'):p.raw_plan(root,root/'missing')
            (root/'prod.keys').write_text('placeholder');(root/'[0100000000000000].nsp').write_bytes(b'placeholder')
            with self.assertRaisesRegex(ValueError,'不是动森'):p.raw_plan(root,root)
    def test_dependency_plan_never_installs(self):
        with patch.object(d.shutil,'which',return_value=None),patch.object(d,'available',return_value=False),patch.object(d.subprocess,'run',side_effect=AssertionError('unexpected install')):
            plan=d.ensure(raw=True,plan=True)
            self.assertTrue(plan['dotnet10']);self.assertIn('spirv-cross',plan['homebrew']);self.assertIn('fonttools',plan['python'])
    def test_cancel_does_not_become_current_directory(self):
        import subprocess
        with patch.object(p.subprocess,'run',return_value=subprocess.CompletedProcess([],130,'','')):
            with self.assertRaisesRegex(ValueError,'取消'):p.picker('game')
if __name__=='__main__':unittest.main()
