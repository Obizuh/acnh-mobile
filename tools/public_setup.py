#!/usr/bin/env python3
"""Build a private official-core IPA with external direct-read ACNH .data resources."""
import argparse,json,os,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);cmd=p.add_subparsers(dest='cmd',required=True)
b=cmd.add_parser('build')
for key in ['patched','romfs','shaders','font','work','output']:b.add_argument('--'+key,type=Path,required=True)
b.add_argument('--shell',type=Path,help='Optional matching game-free IPA or shell ZIP from Releases')
d=cmd.add_parser('data');d.add_argument('--romfs',type=Path,required=True);d.add_argument('--output',type=Path,required=True)
cmd.add_parser('check');a=p.parse_args()
if a.cmd=='check':
 import shutil
 for name in ['xcodebuild','xcodegen']:assert shutil.which(name),'Missing '+name
 print('Build tools found. Use your own rewritten modules, RomFS and matching official shader cache.');sys.exit(0)
if a.cmd=='data':
 subprocess.run([sys.executable,str(ROOT/'tools/pack_data.py'),str(a.romfs.resolve()),str(a.output.resolve())],check=True);sys.exit(0)
a.patched=a.patched.resolve();a.romfs=a.romfs.resolve();a.shaders=a.shaders.resolve();a.font=a.font.resolve();a.work=a.work.resolve();a.output=a.output.resolve()
chain=re.search(r'#define DMSH_CHAIN "([0-9a-f]+)"',(ROOT/'dm/games/01006F8002326000/dmsh_chain.h').read_text()).group(1)
assert (a.shaders/('b-'+chain)).is_dir(),'Expected the matching complete official b-'+chain+' directory under --shaders'
assert a.font.is_file();a.work.mkdir(parents=True,exist_ok=True);a.output.parent.mkdir(parents=True,exist_ok=True)
if a.shell:
 subprocess.run([sys.executable,str(ROOT/'tools/pack_shell.py'),'--shell',str(a.shell.resolve()),'--patched',str(a.patched),'--shaders',str(a.shaders),'--font',str(a.font),'--output',str(a.output),'--control',str(a.romfs.parent/'control.nacp')],check=True);sys.exit(0)
v=json.loads((ROOT/'config/version.json').read_text())
e=dict(os.environ,GAME='01006F8002326000',W=str(a.work),PATCHED=str(a.patched),ROMFS=str(a.romfs),MSL_IOS=str(a.shaders),STD_FONT=str(a.font),FONT_PY=sys.executable,DM_EXTERNAL_ROMFS='1',DM_BUNDLE_ID='org.acnhmobile.experimental',DM_DISPLAY_NAME='ACNH Mobile',DM_APP_VERSION=v['version'],DM_BUILD_NUMBER=str(v['build']),DM_LANG='zh-Hans',DM_ICON=str(ROOT/'dm/ios/Resources/AppIcon.png'))
# Never carry legacy diagnostics or runtime overrides into the official baseline.
for key in ['DM_ENV_FILE','ACNH_RENDER_FLAGS']:e.pop(key,None)
subprocess.run(['bash',str(ROOT/'dm/ios/tools/build_ipa.sh'),str(a.output)],env=e,check=True)
