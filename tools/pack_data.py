#!/usr/bin/env python3
"""Create direct-read .data files from the caller's own RomFS; never extracts at runtime."""
import argparse,hashlib,json,os,struct,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('romfs',type=Path);p.add_argument('output',type=Path);p.add_argument('--part-mib',type=int,default=1024);p.add_argument('--progress',action='store_true');a=p.parse_args()
if not(a.romfs/'Shader/Park_UBER_All.Nin_NX_NVN.szs').is_file():p.error('Not an ACNH RomFS directory')
if a.output.exists()and any(a.output.iterdir()):p.error('Output must be empty')
a.output.mkdir(parents=True,exist_ok=True);limit=a.part_mib*1024**2
if limit<=0:p.error('part size must be positive')
parts=[];files=[];stream=None;offset=0;total_bytes=0;last_report=time.monotonic()
try:
 for f in sorted(a.romfs.rglob('*')):
  if f.is_symlink():raise ValueError('Symlinks are not accepted: '+str(f))
  if not f.is_file():continue
  n=f.stat().st_size
  if stream is None or offset+n>limit:
   if stream:stream.close()
   name=f'ACNH-{len(parts)+1:03d}.data';parts.append({'name':name,'size':0});stream=(a.output/(name+'.partial')).open('wb');offset=0
  h=hashlib.sha256()
  with f.open('rb')as src:
   while chunk:=src.read(1024**2):stream.write(chunk);h.update(chunk)
  files.append({'path':f.relative_to(a.romfs).as_posix(),'part':len(parts)-1,'offset':offset,'size':n,'sha256':h.hexdigest()});offset+=n;parts[-1]['size']=offset;total_bytes+=n
  if a.progress and time.monotonic()-last_report>10:
   print(f'Data packing: {len(files)} files, {total_bytes/1024**3:.1f} GiB',flush=True);last_report=time.monotonic()
finally:
 if stream:stream.close()
for row in parts:os.replace(a.output/(row['name']+'.partial'),a.output/row['name'])
manifest=a.output/'acnh-pack.json';manifest.write_text(json.dumps({'format':'acnh-transfer-v1','parts':parts,'files':files}))
import subprocess,sys
root=Path(__file__).resolve().parents[1];subprocess.run([sys.executable,str(root/'tools/build_data_index.py'),str(manifest),str(a.output/'ACNH-index.data')],check=True)
print('Local pack ready:',a.output)
