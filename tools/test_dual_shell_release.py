"""Security gates for independently built release shells; no game fixtures."""
import json,plistlib,tempfile,unittest,zipfile
from pathlib import Path
from unittest.mock import patch
from prepare_dual_shell_release import inspect

class ShellAuditTests(unittest.TestCase):
    def fixture(self,root,minimum='26.0',game=False,fingerprint='matched',extra=None):
        info={'CFBundleShortVersionString':'0.1.21','CFBundleVersion':'34','MinimumOSVersion':minimum}
        meta={'format':1,'version':'0.1.21','build':34,'minimum_ios':minimum,'source_id':fingerprint,'game_content':game}
        p=root/'shell.zip'
        with zipfile.ZipFile(p,'w')as z:
            for name,data in {'DmIOS':b'synthetic shell','Info.plist':plistlib.dumps(info),'PkgInfo':b'APPL????','dm/env.txt':b'','dm/shell_info.json':json.dumps(meta).encode()}.items():
                z.writestr('Payload/ACNHMobile.app/'+name,data)
            if extra:z.writestr(extra,b'synthetic forbidden fixture')
        return p
    def check(self,p,minimum='26.0',imports=''):
        with patch('prepare_dual_shell_release.audit_app',return_value={}),patch('prepare_dual_shell_release.source_id',return_value='matched'),patch('prepare_dual_shell_release.subprocess.check_output',side_effect=lambda args,**kw:'minos '+minimum+'\n' if args[0]=='otool'else imports):
            return inspect(p,p.parent,minimum,{'version':'0.1.21','build':34})
    def test_valid_shell(self):
        with tempfile.TemporaryDirectory()as t:self.assertFalse(self.check(self.fixture(Path(t)))['game_content'])
    def test_rejects_source_drift(self):
        with tempfile.TemporaryDirectory()as t,self.assertRaisesRegex(ValueError,'fingerprint'):self.check(self.fixture(Path(t),fingerprint='other source'))
    def test_rejects_game_metadata(self):
        with tempfile.TemporaryDirectory()as t,self.assertRaisesRegex(ValueError,'metadata'):self.check(self.fixture(Path(t),game=True))
    def test_rejects_wrong_minimum(self):
        with tempfile.TemporaryDirectory()as t,self.assertRaisesRegex(ValueError,'Info.plist'):self.check(self.fixture(Path(t),minimum='18.0'))
    def test_rejects_path_traversal(self):
        with tempfile.TemporaryDirectory()as t,self.assertRaisesRegex(ValueError,'Unsafe'):self.check(self.fixture(Path(t),extra='Payload/ACNHMobile.app/../../outside'))
    def test_rejects_duplicate_path(self):
        with tempfile.TemporaryDirectory()as t,self.assertRaisesRegex(ValueError,'Unsafe'):self.check(self.fixture(Path(t),extra='Payload/ACNHMobile.app/DmIOS'))
    def test_rejects_compat_metal4(self):
        with tempfile.TemporaryDirectory()as t,self.assertRaisesRegex(ValueError,'Metal 4'):self.check(self.fixture(Path(t),minimum='18.0'),'18.0','_OBJC_CLASS_$_MTL4CommandBuffer')
    def test_actual_allowlist_rejects_extra_resource(self):
        with tempfile.TemporaryDirectory()as t,self.assertRaisesRegex(ValueError,'Unexpected shell files'):
            inspect(self.fixture(Path(t),extra='Payload/ACNHMobile.app/resources/forbidden.dat'),Path(t),'26.0',{'version':'0.1.21','build':34})

if __name__=='__main__':unittest.main()
