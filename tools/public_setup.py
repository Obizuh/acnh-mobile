#!/usr/bin/env python3
"""Local preparation only. No ROMs, keys or remote game downloads."""
import argparse,os,shutil,subprocess,sys,json,re
from check_invariance import check_invariance, check_alpha_test
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];DM=ROOT/'dm';PROFILE=DM/'games/acnh'
def run(args,**kw): print('+',' '.join(map(str,args)),flush=True);subprocess.run(list(map(str,args)),check=True,**kw)
def env():
 e=dict(os.environ,DM_GAME='acnh',GAME='acnh',ACNH_RENDER_FLAGS='-DDM_NVN_M3=1')
 dotnet=shutil.which('dotnet')
 if dotnet and not e.get('DOTNET_ROOT'):
  import re
  runtimes=subprocess.check_output([dotnet,'--list-runtimes'],text=True)
  match=re.search(r'\[([^\]]+)/shared/Microsoft\.NETCore\.App\]',runtimes)
  if match:e['DOTNET_ROOT']=match.group(1)
 return e
def owned_exefs(p):
 expected=dict(line.split('\t')[3].split('=') for line in (PROFILE/'game.tsv').read_text().splitlines()if line.startswith('build\t'))
 for name in ['main','subsdk0','sdk']:
  f=p/name
  if not f.is_file():raise ValueError('Missing owned module: '+str(f))
  b=f.read_bytes()[:96]
  if b[:4]!=b'NSO0':raise ValueError('Expected decrypted NSO0: '+str(f))
  if name=='main'and b[0x40:0x50].hex()!=expected['main']:raise ValueError('This profile supports ACNH 3.0.3 main only')
def main():
 ap=argparse.ArgumentParser(description=__doc__);sub=ap.add_subparsers(dest='command',required=True)
 sub.add_parser('check')
 p=sub.add_parser('rewrite');p.add_argument('--exefs',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p=sub.add_parser('shaders');p.add_argument('--exefs',type=Path,required=True);p.add_argument('--romfs',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p=sub.add_parser('build');p.add_argument('--patched',type=Path,required=True);p.add_argument('--romfs',type=Path,required=True);p.add_argument('--shaders',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--font',type=Path,required=True)
 a=ap.parse_args()
 if a.command=='check':
  missing=[]
  if sys.version_info<(3,10):print('MISS Python 3.10+');missing.append('Python 3.10+')
  else:print('OK   Python 3.10+')
  for name in ['clang','xcodebuild','xcrun','xcodegen','dotnet','spirv-cross','zstd']:
   ok=bool(shutil.which(name));print(('OK   'if ok else 'MISS ')+name)
   if not ok:missing.append(name)
  for name in ['lz4.block','capstone','fontTools']:
   try:__import__(name);print('OK   Python '+name)
   except ImportError:print('MISS Python '+name);missing.append(name)
  return bool(missing)
 if a.command in ('rewrite','shaders'):owned_exefs(a.exefs)
 if a.command=='rewrite':
  a.output.mkdir(parents=True,exist_ok=True)
  for name in ['main','subsdk0','sdk']:run([sys.executable,DM/'tools/patch_nso.py',a.exefs/name,a.output,name],env=env())
 if a.command=='shaders':
  if not(a.romfs/'Shader/Park_UBER_All.Nin_NX_NVN.szs').is_file():raise ValueError('Missing owned ACNH RomFS')
  a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True);e=env();e.update(MAIN=str((a.exefs/'main').resolve()),XLATE=str(a.output/'xlate/dmsh_xlate'),IOSOUT=str(a.output/'ios'))
  run([sys.executable,DM/'gfx/tools/shader_split.py',a.romfs,a.exefs/'main',a.output/'packages',PROFILE/'shader_map.tsv',PROFILE/'shader_facts.tsv',PROFILE/'shader_pairs.tsv'],env=e)
  run(['dotnet','publish',DM/'gfx/tools/dmsh_xlate','-c','Release','-o',a.output/'xlate'],env=e)
  run([sys.executable,DM/'gfx/tools/dmsh_chain.py','--update-lock'],env=e);run([sys.executable,DM/'gfx/tools/dmsh_chain.py','--header'],env=e)
  chain=subprocess.check_output([sys.executable,DM/'gfx/tools/dmsh_chain.py'],env=e,text=True).strip()
  run([sys.executable,DM/'gfx/tools/dmsh_build.py',a.output/'packages',a.output/'mac'],env=e)
  cache=a.output/'mac'/('b-'+chain);ios=a.output/'ios'/('b-'+chain)
  run([sys.executable,DM/'gfx/tools/c1_guard.py',cache,PROFILE/'c1_full.tsv'],env=e)
  run([sys.executable,PROFILE/'tools/rebuild_position_invariance.py',cache,a.output/'invariance'],env=e)
  run([sys.executable,PROFILE/'tools/rebuild_alpha_test.py','--cache',cache,'--output',a.output/'alpha-test'],env=e)
  for change in ['invariance','alpha-test']:
   for platform,dest in [('mac',cache),('ios',ios)]:
    for f in (a.output/change/platform).glob('*.metallib'):shutil.copy2(f,dest/f.name)
   for f in (a.output/change).glob('*.json'):
    if f.name=='manifest.json':continue
    for dest in [cache,ios]:shutil.copy2(f,dest/f.name)
  shutil.copy2(a.output/'invariance/manifest.json',ios/'position-invariance.json')
  shutil.copy2(a.output/'alpha-test/manifest.json',ios/'alpha-test.json')
  check_invariance(ios)
  check_alpha_test(ios)
  print('Prepared locally:',a.output)
 if a.command=='build':
  if not a.font.is_file():raise ValueError('Missing user-provided standard font: '+str(a.font))
  chain=re.search(r'#define DMSH_CHAIN "([0-9a-f]+)"',(PROFILE/'dmsh_chain.h').read_text()).group(1)
  check_invariance(a.shaders/('b-'+chain))
  check_alpha_test(a.shaders/('b-'+chain))
  e=env();e.update(PATCHED=str(a.patched.resolve()),ROMFS=str(a.romfs.resolve()),MSL_IOS=str(a.shaders.resolve()),W=str(a.work.resolve()),STD_FONT=str(a.font.resolve()),FONT_PY=sys.executable,DM_ENV_FILE=str(ROOT/'config/runtime.env'))
  a.output=a.output.resolve();a.output.parent.mkdir(parents=True,exist_ok=True)
  run(['bash',DM/'ios-acnh/tools/build_ipa.sh',a.output],env=e)
  print('Private local build only: embedded game modules and translated shaders are in this IPA. Do not include it in the public source release.')
 return 0
if __name__=='__main__':
 try:sys.exit(main())
 except(ValueError,subprocess.CalledProcessError)as e:sys.exit(str(e))
