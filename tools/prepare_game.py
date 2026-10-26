#!/usr/bin/env python3
"""Prepare ACNH 3.0.3 using upstream extraction, rewrite and Metal shader tools."""
import hashlib,importlib.util,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
TITLE='01006F8002326000'

def picker(kind):
    steps={'game':(1,'动森游戏目录','请选择包含本体和 3.0.3 更新 NSP 的文件夹。'),
           'keys':(2,'prod.keys 文件','请选择 prod.keys 文件，不要选择文件夹或 title.keys。'),
           'output':(3,'成品保存文件夹','请选择生成结果的保存位置，可以新建文件夹。')}
    number,subject,detail=steps[kind]
    if sys.stdout.isatty() and os.environ.get('TERM')!='dumb' and 'NO_COLOR' not in os.environ:
        subject='\033[1;31m'+subject+'\033[0m'
    print('\n第 '+str(number)+' 步 / 共 3 步：选择 '+subject+'\n'+detail,flush=True)
    result=subprocess.run(['xcrun','swift',ROOT/'tools/choose_build_input.swift',kind],capture_output=True,text=True)
    if result.returncode==130:raise ValueError('已取消选择，未生成或删除任何安装包。')
    if result.returncode:raise ValueError('无法打开文件选择窗口：'+result.stderr)
    p=Path(result.stdout.strip()).expanduser().resolve()
    if not p.exists():raise ValueError('选择的文件或目录不存在。')
    return p

def load_pipeline():
    spec=importlib.util.spec_from_file_location('upstream_nsp',ROOT/'tools/upstream/nsp_pipeline.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    module.BuildError=ValueError
    return module

def raw_plan(game,keys):
    game=Path(game).expanduser().resolve();keys=Path(keys).expanduser().resolve()
    if keys.is_dir():keys=keys/'prod.keys'
    if not keys.is_file():raise ValueError('请选择自己的 prod.keys 文件。')
    u=load_pipeline();app,family=u.scan_nsps(game)
    if app!=int(TITLE,16):raise ValueError('所选目录不是动森，请选择仅含动森的目录。')
    if len(family['updates'])!=1:raise ValueError('目录中请只保留一个 3.0.3 更新 NSP，避免选错版本。')
    version,update=family['updates'][0]
    if version is not None and version!=2228224:raise ValueError('当前仅支持动森 3.0.3 更新。')
    return u,game,keys,family

def cache_key(family):
    # Fingerprint input contents, never key values. A new input set gets a separate cache.
    from build_shell import source_id
    h=hashlib.sha256(source_id(ROOT).encode())
    for p in [family['base'],family['updates'][0][1]]:
        h.update(p.name.encode());h.update(str(p.stat().st_size).encode());h.update(str(p.stat().st_mtime_ns).encode())
    for base in ('tools','dm/tools','dm/gfx/tools','dm/games/'+TITLE,'dm/third_party/ryujinx'):
        for p in sorted((ROOT/base).rglob('*')):
            if p.is_file() and p.name not in ('c1_full.tsv','cb_extent.tsv') and p.suffix in ('.py','.cs','.csproj','.tsv','.h','.lock') and not any(x in ('obj','bin','__pycache__') for x in p.parts):h.update(p.read_bytes())
    return h.hexdigest()[:20]

def prepare(game,keys,work,run,log,platform='ios'):
    u,game,keys,family=raw_plan(game,keys)
    cache=Path(work)/'prepared'/cache_key(family);cache.mkdir(parents=True,exist_ok=True)
    repo=cache/'source';manifest=cache/('prepared-mac.json' if platform=='mac' else 'prepared.json')
    if manifest.is_file():
        data=json.loads(manifest.read_text())
        from shader_tables import chain_header,ensure_tables
        prepared_root=Path(data['build_root'])
        if chain_header(prepared_root)!=data['chain']:raise ValueError('Prepared shader chain changed; prepare the game again')
        ensure_tables(prepared_root,Path(data['shaders']),data['chain'])
        shutil.copyfile(ROOT/'config/version.json',prepared_root/'config/version.json')
        return {k:Path(v) if k!='chain' else v for k,v in data.items()}
    if not repo.exists():
        shutil.copytree(ROOT,repo,ignore=shutil.ignore_patterns('.git','.venv','work','local','outputs','__pycache__','Gen','gen','bin','obj','build','*.xcodeproj','*.xcworkspace','*.dSYM','*.local.json'))
    shutil.copyfile(ROOT/'config/version.json',repo/'config/version.json')
    print('私人准备缓存与日志：'+str(cache),flush=True)
    # All regenerated metadata stays in the private source copy, never the Git checkout.
    env=dict(os.environ,DM_GAME=TITLE,DM_ROOT=str(cache),WORK=str(cache/'tmp'),J=str(min(os.cpu_count() or 4,8)))
    for key in ('DM_GAMES_DIR','DM_GAMES_FIRST','DM_PATCH_FRONT','XLATE_DEBUG','XLATE_REDUCED','ACNH_RENDER_FLAGS','DM_ENV_FILE'):env.pop(key,None)
    (cache/'tmp').mkdir(exist_ok=True);logs=cache/'logs';logs.mkdir(exist_ok=True)
    sys.path.insert(0,str(repo/'dm/games'));import game as games
    g=games.load(TITLE);u.PROG.mode='lines';u.PROG.plan(6)
    u.PROG.stage('build extraction tool')
    nx=u.build_nx_extract(repo,cache,logs,env);u.PROG.end()
    ex=u.extract(nx,keys,cache/'extract','3.0.3',family['base'],family['updates'][0][1],None,logs,g)
    bad=u.build_mismatch(ex,'v2228224',g)
    if bad:raise ValueError('游戏版本不匹配：'+', '.join(bad))
    patched=cache/'patched';u.rewrite(sys.executable,repo,ex/'ExeFS',patched,env,logs,g)
    xlate=repo/'dm/gfx/tools/dmsh_xlate_bin/dmsh_xlate'
    u.PROG.stage('build official shader translator')
    u.run(['dotnet','publish',repo/'dm/gfx/tools/dmsh_xlate','-c','Release','-o',xlate.parent],logs/'translator.log',env=env);u.PROG.end()
    env['XLATE']=str(xlate)
    for flag in ('--update-lock','--header'):
        u.run([sys.executable,repo/'dm/gfx/tools/dmsh_chain.py',flag],logs/('chain'+flag+'.log'),env=env)
    sh=cache/'shader'
    key=u.split_shaders(sys.executable,repo,[ex/'RomFS'],[ex/'ExeFS/main'],sh/'pkg',env,logs,g)
    ios=u.build_shaders(sys.executable,repo,sh,xlate,env,logs,g,pkg_key=key,mac=platform=='mac')
    chain=u.chain_of(repo)   # build_shaders also generates/validates both local tables, even on reuse
    data=dict(patched=patched,romfs=ex/'RomFS',shaders=ios,build_root=repo,chain=chain)
    # Written only after every stage succeeds; later IPA construction validates the cache again.
    manifest.write_text(json.dumps({k:str(v) for k,v in data.items()}))
    return data
