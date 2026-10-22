#!/usr/bin/env python3
"""Verify the shell archive, source identity, version, installation minimum and checksum."""
import hashlib,json,plistlib,re,subprocess,sys,tempfile
from pathlib import Path
from pack_shell import extract_shell
from build_shell import ROOT,audit_app,source_id
out=Path(sys.argv[1]);v=json.loads((ROOT/'config/version.json').read_text())
ipa=out/('ACNHMobile-iOS26-v'+v['version']+'-build'+str(v['build'])+'-shell-unsigned.ipa')
assert {p.name for p in out.iterdir()}=={ipa.name,'shell-audit.json','SHA256SUMS.txt'},'Unexpected or stale release assets'
for line in (out/'SHA256SUMS.txt').read_text().splitlines():
    digest,name=line.split('  ',1)
    assert Path(name).name==name and hashlib.sha256((out/name).read_bytes()).hexdigest()==digest,'Checksum mismatch'
with tempfile.TemporaryDirectory() as tmp:
    extract_shell(ipa,Path(tmp))
    app=Path(tmp)/'Payload/ACNHMobile.app';audit_app(app)
    info=plistlib.loads((app/'Info.plist').read_bytes());meta=json.loads((app/'dm/shell_info.json').read_text())
    assert info['CFBundleShortVersionString']==v['version'] and info['CFBundleVersion']==str(v['build'])
    assert info['MinimumOSVersion']=='26.0'
    assert meta['source_id']==source_id(ROOT) and meta['game_content'] is False
    commands=subprocess.check_output(['otool','-l',app/'DmIOS'],text=True)
    assert re.search(r'\bminos\s+26\.0\b',commands)
    assert b'MTL4' in (app/'DmIOS').read_bytes(),'Official Metal 4 runtime absent'
print('Verified game-free official Metal 4 shell, version/build, source identity and SHA256')
