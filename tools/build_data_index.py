from pathlib import Path
import argparse,json,struct,hashlib
p=argparse.ArgumentParser(description='Build a memory-mapped direct-read index for existing ACNH .data parts (no extraction).');p.add_argument('manifest',type=Path);p.add_argument('output',type=Path);a=p.parse_args();d=json.loads(a.manifest.read_text());assert d['format']=='acnh-transfer-v1';parts=d['parts'];files=sorted(d['files'],key=lambda x:x['path'].encode());assert 0<len(parts)<=100 and 0<len(files)<=500000
strings=bytearray();pr=bytearray();fr=bytearray();seen=set()
def string(s):
 b=s.encode('utf-8');assert b'\0' not in b;o=len(strings);strings.extend(b+b'\0');return o
for row in parts:
 name=row['name'];assert Path(name).name==name and name.endswith('.data') and row['size']>=0
 pr+=struct.pack('<QQ',string(name),row['size'])
for row in files:
 path=row['path'];assert path not in seen and not path.startswith('/') and all(c not in ('','.','..') for c in path.split('/')) and '\\' not in path;seen.add(path)
 part=row['part'];off=row['offset'];n=row['size'];assert 0<=part<len(parts) and 0<=off<=parts[part]['size'] and 0<=n<=parts[part]['size']-off
 fr+=struct.pack('<QQQII32s',string(path),off,n,part,0,bytes.fromhex(row['sha256']))
assert 'Shader/Park_UBER_All.Nin_NX_NVN.szs' in seen
body=pr+fr+strings;head=struct.pack('<8sIIQQQQQQ',b'ACNHDAT1',1,len(parts),len(files),96,96+len(pr),96+len(pr)+len(fr),len(strings),96+len(body));assert len(head)==64
result=head+hashlib.sha256(body).digest()+body;a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(result);print('index',a.output,'bytes',len(result),'files',len(files),'parts',len(parts),'sha256',hashlib.sha256(result).hexdigest())
