"""Run production argument-buffer GPU checks; all outputs stay in a chosen folder."""
from pathlib import Path
import argparse,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);a=p.parse_args();work=a.work.resolve();work.mkdir(parents=True,exist_ok=True)
 base=ROOT/'dm/gfx'
 subprocess.run([sys.executable,str(ROOT/'dm/games/game.py'),'header','acnh',str(work/'game_profile.h')],check=True)
 src=sorted([*base.glob('nvn/*.c'),*base.glob('nvn/*.m'),*base.glob('tex/*.c'),*base.glob('tex/*.m')]);src=[p for p in src if p.name not in {'nvn_mtl.m','hle_nvn.c'}]
 cmd=['clang','-O1','-g','-DDM_NVN_M3=1','-fobjc-arc','-Wno-unused-function','-I'+str(base/'nvn/include'),'-I'+str(work),'-I'+str(ROOT/'dm/games/acnh'),'-o',str(work/'t_argument_encoder'),str(base/'tests/t_argument_encoder.m'),str(ROOT/'dm/host/crash.c'),*map(str,src)]
 for fw in ['Metal','Foundation','QuartzCore','ImageIO','CoreGraphics','UniformTypeIdentifiers']:cmd+=['-framework',fw]
 with (work/'build.log').open('w')as f:subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,check=True)
 with (work/'test.log').open('w')as f:subprocess.run([str(work/'t_argument_encoder')],stdout=f,stderr=subprocess.STDOUT,check=True)
 print((work/'test.log').read_text())
if __name__=='__main__':main()
