#!/usr/bin/env python3
"""Keep translator and missing-tool failures strict while allowing local compilers."""
import contextlib,io,runpy,sys,types,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
class OwnCompilerTests(unittest.TestCase):
    def check(self,issues):
        chain=types.ModuleType('dmsh_chain');chain.check_tools=lambda:issues
        before=list(sys.path);argv=list(sys.argv)
        try:
            with patch.dict(sys.modules,{'dmsh_chain':chain}),patch.object(runpy,'run_path') as launch,contextlib.redirect_stdout(io.StringIO()):
                code=compile((ROOT/'dm/gfx/tools/own_compilers.py').read_text(),str(ROOT/'dm/gfx/tools/own_compilers.py'),'exec');scope={'__file__':str(ROOT/'dm/gfx/tools/own_compilers.py')};exec(code,scope);result=chain.check_tools();launch.assert_called_once();return result
        finally:sys.path[:]=before;sys.argv[:]=argv
    def test_local_metal_and_cross_allowed(self):
        self.assertEqual(self.check(['metal_iphoneos: lock old / actual new','metal_macosx: lock old / actual new','spirv-cross: lock old / actual new']),[])
    def test_missing_compiler_and_translator_remain_errors(self):
        issues=['metal_iphoneos: lock old / actual missing','dmsh_xlate: lock old / actual new'];self.assertEqual(self.check(issues),issues)
    def test_other_errors_are_not_suppressed(self):
        self.assertEqual(self.check(['unknown mismatch']),['unknown mismatch'])
if __name__=='__main__':unittest.main()
