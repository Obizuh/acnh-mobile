from pathlib import Path
import json,subprocess,concurrent.futures,time,hashlib
import argparse
parser=argparse.ArgumentParser(description="Recompile ACNH vertex shaders with Metal position invariance; preserve the base shader-chain inputs.")
parser.add_argument("source",type=Path,help="Existing b-<chain> cache containing .metal sources")
parser.add_argument("output",type=Path,help="Separate destination for repaired mac/ios libraries and audit manifest")
args=parser.parse_args();src=args.source;out=args.output;out.mkdir(parents=True,exist_ok=True)
items=[p for p in src.glob('*_b.metal') if 'vertex main0_out main0(' in p.read_text()]
print('vertex shader count',len(items),flush=True)
def job(p):
 s=p.read_text()
 if '[[position, invariant]]' not in s:
  if '[[position]]' not in s:raise ValueError('No vertex position output: '+str(p))
  s=s.replace('[[position]]','[[position, invariant]]');p.write_text(s)
 result={'source_sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
 for platform,sdk in [('mac','macosx'),('ios','iphoneos')]:
  d=out/platform;d.mkdir(exist_ok=True);air=d/(p.stem+'.air');lib=d/(p.stem+'.metallib');cmd=['xcrun','-sdk',sdk,'metal','-c','-fpreserve-invariance']
  if sdk=='iphoneos':cmd+=['-mios-version-min=18.0']
  q=subprocess.run(cmd+[str(p),'-o',str(air)],capture_output=True,text=True)
  if q.returncode:raise RuntimeError(str(p)+' '+q.stderr[-1000:])
  q=subprocess.run(['xcrun','-sdk',sdk,'metallib',str(air),'-o',str(lib)],capture_output=True,text=True)
  if q.returncode:raise RuntimeError(str(p)+' '+q.stderr[-1000:])
  air.unlink();result[platform]=hashlib.sha256(lib.read_bytes()).hexdigest()
 return p.stem,result
start=time.monotonic();results={}
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
 for i,(key,result) in enumerate(pool.map(job,items),1):
  results[key]=result
  if i%100==0:print(i,'/',len(items),'seconds',round(time.monotonic()-start,1),flush=True)
(out/'manifest.json').write_text(json.dumps(dict(flags=['-fpreserve-invariance'],shaders=results,seconds=time.monotonic()-start),indent=2));print('COMPLETE',len(results),flush=True)
