"""Keep locally generated NVN metadata in step with the matching shader build.

Adapted from NXNative chain_header/chain_tables (a2e6f4a, 4c7adf4) for the
prepared-input and shell-fill entry points retained by this project.
"""
import os
import json
from pathlib import Path
import re
import subprocess
import sys

TITLE = '01006F8002326000'
TABLES = (('c1_full.tsv', 'c1_guard.py'), ('cb_extent.tsv', 'cb_extent.py'))


def validate_cache(root, shaders, chain):
    profile = Path(root) / 'dm/games' / TITLE
    wanted = {c[3] for line in (profile / 'shader_map.tsv').read_text().splitlines()
              if len(c := line.split('\t')) > 4 and c[0].endswith('.bin') and c[2] != 'geometry'}
    cache = Path(shaders) / ('b-' + chain)
    libs = list(cache.glob('*_b.metallib'))
    if not wanted or wanted - {p.name[:-11] for p in libs}:
        raise ValueError('着色器构建尚未完成或缓存不完整：' + str(cache))
    for lib in libs:
        if not lib.with_suffix('.json').is_file() or json.loads(lib.with_suffix('.json').read_text()).get('chain') != chain:
            raise ValueError('着色器配套元数据缺失或链不匹配：' + lib.name)


def chain_header(root):
    root = Path(root)
    env = dict(os.environ, DM_GAME=TITLE)
    for key in ('DM_GAMES_DIR', 'DM_GAMES_FIRST', 'XLATE_DEBUG', 'XLATE_REDUCED'):
        env.pop(key, None)
    tool = root / 'dm/gfx/tools/dmsh_chain.py'
    want = subprocess.check_output([sys.executable, str(tool)], env=env, text=True).strip()
    if not re.fullmatch(r'[0-9a-f]{12}', want):
        raise ValueError('Invalid shader chain: ' + want)
    header = root / 'dm/games' / TITLE / 'dmsh_chain.h'
    if not header.is_file() or f'#define DMSH_CHAIN "{want}"' not in header.read_text():
        subprocess.run([sys.executable, str(tool), '--header'], env=env, check=True)
    return want


def ensure_tables(root, ios, chain, mac=None):
    root, ios = Path(root), Path(ios)
    profile = root / 'dm/games' / TITLE
    missing = []
    for name, tool in TABLES:
        p = profile / name
        head = p.read_text().partition('\n')[0] if p.is_file() else ''
        if head.split()[:3] != ['#', 'chain', 'b-' + chain]:
            missing.append((p, tool))
    if not missing:
        return
    candidates = ([Path(mac)] if mac else []) + [ios.parent / 'cache', ios.parent / 'mac', ios]
    outputs = next((p / ('b-' + chain) for p in candidates
                    if next((p / ('b-' + chain)).glob('*_b.metal'), None)), None)
    if outputs is None:
        raise ValueError('缺少或过期的着色器配套表；需要同次构建的 Mac 输出生成。'
                         '请提供 --mac-shaders（包含 b-' + chain + ' 的目录），或重新准备游戏。')
    # Do not mark a table current from a shader build that is still in progress.
    validate_cache(root, outputs.parent, chain)
    for p, tool in missing:
        subprocess.run([sys.executable, str(root / 'dm/gfx/tools' / tool), str(outputs), str(p)], check=True)
        if p.read_text().partition('\n')[0].split()[:3] != ['#', 'chain', 'b-' + chain]:
            raise ValueError('Generated shader table has the wrong chain: ' + p.name)
