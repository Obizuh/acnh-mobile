"""Reject shader packs that dropped ACNH vertex position-invariance fixes."""
import hashlib,json
from pathlib import Path

def check_invariance(directory):
 directory=Path(directory)
 report=directory/'position-invariance.json'
 if not report.is_file():raise ValueError('Missing position-invariance.json; regenerate shaders with ./setup.sh shaders before building')
 shaders=json.loads(report.read_text()).get('shaders',{})
 vertices={p.stem for p in directory.glob('*_b.json') if json.loads(p.read_text()).get('stage')=='vs'}
 if not vertices or not vertices.issubset(shaders):raise ValueError('Incomplete vertex position-invariance manifest')
 for key in vertices:
  lib=directory/(key+'.metallib')
  if not lib.is_file() or hashlib.sha256(lib.read_bytes()).hexdigest()!=shaders[key].get('ios'):raise ValueError('Vertex position-invariance library mismatch: '+key)
 return len(vertices)
