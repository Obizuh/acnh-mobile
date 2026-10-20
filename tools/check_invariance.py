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

ALPHA_KEYS = ('1bbd8e6baf1769fe_b', '9aaab9f97ef18924_b', '5c518b71b8502c40_b')

def check_alpha_test(directory):
 directory=Path(directory)
 report=directory/'alpha-test.json'
 if not report.is_file():raise ValueError('Missing alpha-test.json; regenerate ACNH shaders before building')
 manifest=json.loads(report.read_text())
 for key in ALPHA_KEYS:
  lib=directory/(key+'.metallib');meta=directory/(key+'.json')
  record=manifest.get(key,{})
  if not lib.is_file() or hashlib.sha256(lib.read_bytes()).hexdigest()!=record.get('ios'):
   raise ValueError('UI alpha-test library mismatch: '+key)
  j=json.loads(meta.read_text())
  if j.get('acnh_alpha_test') is not True or j.get('acnh_alpha_test_buffer')!=30:
   raise ValueError('UI alpha-test metadata missing: '+key)
 return 3
