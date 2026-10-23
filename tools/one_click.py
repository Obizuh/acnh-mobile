#!/usr/bin/env python3
"""Local launchers for private IPA, IPA plus external data, and game-free shell."""
import argparse,fcntl,hashlib,json,os,platform,plistlib,re,shutil,struct,subprocess,sys,time,uuid,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PROFILE=ROOT/'dm/games/01006F8002326000'
sys.path.insert(0,str(ROOT/'tools'))
CONFIG=ROOT/'local-build.local.json'
MODULES=('main','subsdk0','sdk')
MARKER='Shader/Park_UBER_All.Nin_NX_NVN.szs'

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        while chunk:=f.read(8*1024*1024):h.update(chunk)
    return h.hexdigest()

def choose(values,description):
    for p in values:
        if p and Path(p).exists():return Path(p).expanduser().resolve()
    raise ValueError('找不到'+description+'，请在 local-build.local.json 中设置路径。')

def data_root(settings):
    explicit=os.environ.get('ACNH_DATA_ROOT') or settings.get('data_root')
    if explicit:return choose([Path(explicit).expanduser()],'游戏文件目录')
    candidates=sorted(Path('/Volumes').glob('*/动森'))+[Path.home()/'Downloads/动森']
    return choose(candidates,'游戏文件目录')

def inputs(settings):
    base=data_root(settings);build=base/'01006F8002326000_build'
    root=Path(settings.get('build_root') or ROOT);profile=root/'dm/games/01006F8002326000'
    patched=choose([settings.get('patched'),build/'pristine_upstream/patched'],'官方改写程序')
    romfs=choose([settings.get('romfs'),build/'extract/3.0.3/RomFS'],'3.0.3 RomFS')
    shaders=choose([settings.get('shaders'),build/'upstream_shader/ios'],'官方 iOS 着色器缓存')
    font=choose([settings.get('font'),Path('/System/Library/Fonts/Supplemental/Arial Unicode.ttf')],'字体')
    chain=re.search(r'#define DMSH_CHAIN "([0-9a-f]+)"',(profile/'dmsh_chain.h').read_text()).group(1)
    cache=shaders/('b-'+chain)
    if not (romfs/MARKER).is_file():raise ValueError('RomFS 不完整或不是动森。')
    expected={}
    for line in (profile/'game.tsv').read_text().splitlines():
        if line.startswith('build\t'):
            expected.update(x.split('=',1) for x in line.split('\t')[3:])
    for name in MODULES:
        for suffix in ('patched.bin','fixups.tsv','gates.tsv','cuts.tsv','obs.tsv'):
            if not (patched/(name+'.'+suffix)).is_file():raise ValueError('改写程序缺少 '+name+'.'+suffix)
        header=(patched/(name+'.fixups.tsv')).read_text()
        match=re.search(r'^# BUILD_ID\s+(\w+)',header,re.M)
        if not match or match[1]!=expected[name]:raise ValueError('游戏程序版本不匹配：'+name)
    provenance=patched/'build_src.sha256'
    if not provenance.is_file():raise ValueError('缺少官方改写来源校验记录 build_src.sha256。')
    for line in provenance.read_text().splitlines():
        digest,name=line.split(None,1)
        if Path(name).name!=name or sha(root/'dm/tools'/name)!=digest:raise ValueError('改写工具已变化，请重新准备程序。')
    libs=list(cache.glob('*_b.metallib'))
    wanted={c[3] for line in (profile/'shader_map.tsv').read_text().splitlines() if len(c:=line.split('\t'))>4 and c[0].endswith('.bin') and c[2]!='geometry'}
    if wanted-{p.name[:-11] for p in libs}:raise ValueError('官方着色器缓存不完整：b-'+chain)
    for lib in libs:
        reflection=json.loads(lib.with_suffix('.json').read_text())
        if reflection.get('chain')!=chain:raise ValueError('着色器混包：'+lib.name)
    try:
        from fontTools.ttLib import TTFont
    except ImportError:raise ValueError('缺少 FontTools，请使用已安装 requirements.txt 的 Python 环境。')
    with TTFont(font):pass
    return dict(patched=patched,romfs=romfs,shaders=shaders,font=font,chain=chain,shader_libraries=len(libs),build_root=root)

def inspect_ipa(path,version,kind):
    with zipfile.ZipFile(path) as z:
        if z.testzip():raise ValueError('IPA 完整性检查失败。')
        names=z.namelist();info_name=next(n for n in names if re.fullmatch(r'Payload/[^/]+\.app/Info.plist',n))
        info=plistlib.loads(z.read(info_name))
        if (info.get('CFBundleShortVersionString'),info.get('CFBundleVersion'))!=(version['version'],str(version['build'])):raise ValueError('IPA 版本号不匹配。')
        if info.get('CFBundleIdentifier')!='org.acnhmobile.experimental' or info.get('MinimumOSVersion')!='26.0':raise ValueError('IPA 应用标识或系统版本不匹配。')
        if any('/romfs/' in n.lower() for n in names):raise ValueError('RomFS 不应装入 IPA。')
        if kind=='private' and not any('/msl/' in n and n.endswith('.metallib') for n in names):raise ValueError('完整 IPA 缺少已转换的着色器。')
    return sha(path)

def romfs_fingerprint(root):
    h=hashlib.sha256();count=0
    for p in sorted(root.rglob('*')):
        if p.is_symlink():raise ValueError('RomFS 不接受符号链接。')
        if p.is_file():
            s=p.stat();h.update((p.relative_to(root).as_posix()+'\0'+str(s.st_size)+'\0'+str(s.st_mtime_ns)+'\n').encode());count+=1
    return h.hexdigest(),count

def verify_data(folder):
    index=folder/'ACNH-index.data'
    if index.is_symlink() or not 96<=index.stat().st_size<=128*1024*1024:raise ValueError('数据索引大小或类型无效。')
    raw=index.read_bytes()
    if len(raw)<96:raise ValueError('数据索引不完整。')
    magic,version,np,nf,po,fo,so,ss,total=struct.unpack_from('<8sIIQQQQQQ',raw)
    if not(magic==b'ACNHDAT1' and version==1 and 0<np<=100 and 0<nf<=500000 and total==len(raw) and po==96 and fo==po+np*16 and so==fo+nf*64 and so<=total and ss==total-so and hashlib.sha256(raw[96:]).digest()==raw[64:96]):raise ValueError('数据索引格式或校验和错误。')
    def string(off):
        if off>=ss:raise ValueError('数据索引字符串越界。')
        end=raw.find(b'\0',so+off)
        if end<0:raise ValueError('数据索引字符串损坏。')
        return raw[so+off:end].decode('utf-8')
    parts=[];handles=[];seen=set();last=time.monotonic()
    try:
        for i in range(np):
            no,size=struct.unpack_from('<QQ',raw,po+i*16);name=string(no);p=folder/name
            if Path(name).name!=name or not name.endswith('.data') or p.is_symlink() or p.stat().st_size!=size:raise ValueError('数据分卷缺失或大小错误。')
            parts.append(size);handles.append(p.open('rb'))
        for i in range(nf):
            no,off,size,part,reserved,digest=struct.unpack_from('<QQQII32s',raw,fo+i*64);name=string(no)
            if name in seen or name.startswith('/') or '\\' in name or any(s in ('','.','..') for s in name.split('/')) or part>=np or off>parts[part] or size>parts[part]-off:raise ValueError('数据文件条目无效。')
            seen.add(name);f=handles[part];f.seek(off);h=hashlib.sha256();left=size
            while left:
                chunk=f.read(min(left,1024*1024))
                if not chunk:raise ValueError('数据分卷意外结束。')
                left-=len(chunk);h.update(chunk)
            if h.digest()!=digest:raise ValueError('数据内容校验失败：'+name)
            if time.monotonic()-last>10:print('数据校验：'+str(i+1)+' / '+str(nf),flush=True);last=time.monotonic()
        if MARKER not in seen:raise ValueError('数据包缺少动森标识文件。')
    finally:
        for f in handles:f.close()
    return nf,np

def trash_file(path):
    # Only generated IPA files selected by this tool are moved; no data/save removal.
    device=path.stat().st_dev;home=Path.home();volume=None
    for parent in path.parents:
        if parent.parent==Path('/Volumes'):volume=parent;break
    trash=(volume/'.Trashes'/str(os.getuid())) if volume and device!=home.stat().st_dev else home/'.Trash'
    trash.mkdir(parents=True,exist_ok=True)
    target=trash/(path.stem+'-'+uuid.uuid4().hex[:8]+path.suffix)
    path.rename(target)
    side=path.with_suffix('.ipa.sha256')
    if side.is_file():side.rename(target.with_suffix('.ipa.sha256'))

def ipa_identity(path):
    with zipfile.ZipFile(path) as z:
        name=next(n for n in z.namelist() if re.fullmatch(r'Payload/[^/]+\.app/Info.plist',n))
        info=plistlib.loads(z.read(name))
        if info.get('CFBundleIdentifier')!='org.acnhmobile.experimental':raise ValueError('Not this app')
        has_game=any('/msl/' in n and n.endswith('.metallib') or '/Frameworks/Guest.framework/Guest' in n for n in z.namelist())
        kind='shell' if any(n.endswith('/dm/shell_info.json') for n in z.namelist()) and not has_game else 'private'
        return info['CFBundleShortVersionString'],info['CFBundleVersion'],kind

def retain_two(output,newest):
    identity=ipa_identity(newest);others=[]
    for p in output.rglob('ACNHMobile*.ipa'):
        if p==newest or p.is_symlink():continue
        try:other=ipa_identity(p)
        except (ValueError,KeyError,StopIteration,zipfile.BadZipFile):continue
        others.append((p,other))
    others.sort(key=lambda row:row[0].stat().st_mtime_ns,reverse=True)
    previous=next((p for p,other in others if other!=identity),None)
    for p,_ in others:
        if p!=previous:trash_file(p)

def run(command,log):
    with subprocess.Popen([str(x) for x in command],cwd=ROOT,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1) as proc:
        for line in proc.stdout:print(line,end='',flush=True);log.write(line);log.flush()
        if proc.wait():raise RuntimeError('构建失败，请查看日志。')

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('mode',choices=['ipa','ipa-data','shell']);ap.add_argument('--plan',action='store_true');ap.add_argument('--select',action='store_true',help='选择游戏目录和 prod.keys');ap.add_argument('--game-dir',type=Path);ap.add_argument('--keys',type=Path);ap.add_argument('--config',type=Path,default=CONFIG);a=ap.parse_args()
    settings=json.loads(a.config.read_text()) if a.config.is_file() else {}
    python=settings.get('python')
    if python and not os.environ.get('ACNH_DEPENDENCIES_READY') and Path(python).absolute()!=Path(sys.executable).absolute():os.execv(python,[python,str(Path(__file__).resolve()),*sys.argv[1:]])
    if platform.system()!='Darwin' or platform.machine()!='arm64':raise ValueError('需要 Apple Silicon Mac。')
    import build_dependencies
    raw=a.mode!='shell' and (a.select or a.game_dir is not None or settings.get('input_mode')=='raw' or not (os.environ.get('ACNH_DATA_ROOT') or any(settings.get(k) for k in ('data_root','patched','romfs','shaders'))))
    if not a.plan:build_dependencies.ensure(raw=raw)
    output=Path(settings.get('output_root') or ROOT/'outputs').expanduser().resolve()
    work=Path(settings.get('work_root') or ROOT/'work/local-build').expanduser().resolve()
    version=json.loads((ROOT/'config/version.json').read_text());data={}
    if raw:
        from prepare_game import picker,raw_plan,prepare
        game=a.game_dir or (picker('game') if a.select or not settings.get('game_dir') else Path(settings['game_dir']))
        keys=a.keys or (picker('keys') if a.select or not settings.get('keys') else Path(settings['keys']))
        raw_plan(game,keys)
    elif a.mode!='shell':data=inputs(settings)
    if a.plan:
        print(json.dumps(dict(mode=a.mode,version=version,output=str(output),work=str(work),inputs={k:str(v) for k,v in data.items()},dependencies=build_dependencies.ensure(raw=raw,plan=True),raw=raw),ensure_ascii=False,indent=2));return
    output.mkdir(parents=True,exist_ok=True);work.mkdir(parents=True,exist_ok=True)
    with (work/'build.lock').open('w') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise ValueError('已有一个一键构建在运行，请等待结束。')
        job=work/(time.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:6]);job.mkdir()
        print('开始构建 '+version['version']+' / 构建 '+str(version['build']),flush=True)
        print('日志：'+str(job/'run.log'),flush=True)
        kind='shell' if a.mode=='shell' else 'private'
        with (job/'run.log').open('w') as log:
            if raw:
                prepared=prepare(game,keys,work,run,log)
                settings.update({k:str(v) for k,v in prepared.items()});settings['data_root']=str(game)
                data=inputs(settings)
            if kind=='shell':
                run([sys.executable,ROOT/'tools/build_shell.py','--work',job/'shell-work','--output',job/'shell-output'],log)
                run([sys.executable,ROOT/'tools/verify_shell.py',job/'shell-output'],log)
                made=next((job/'shell-output').glob('*.ipa'))
            else:
                made=job/'complete.ipa';command=[sys.executable,data['build_root']/'tools/public_setup.py','build']
                for key in ('patched','romfs','shaders','font'):command+=['--'+key,data[key]]
                command+=['--work',job/'build-work','--output',made];run(command,log)
            digest=inspect_ipa(made,version,kind)
            if a.mode=='ipa-data':
                target=output/'ACNHMobile';fingerprint,count=romfs_fingerprint(data['romfs']);marker=target/'.source.json'
                reuse=marker.is_file() and json.loads(marker.read_text()).get('fingerprint')==fingerprint
                if not reuse:
                    if target.exists():raise ValueError('已有数据目录与当前 RomFS 来源不一致，未覆盖。请选择另一输出目录。')
                    stage=job/'data-stage';print('生成外置 .data（不装入 IPA）…',flush=True)
                    run([sys.executable,ROOT/'tools/pack_data.py',data['romfs'],stage,'--progress'],log)
                    n,parts=verify_data(stage)
                    if n!=count or romfs_fingerprint(data['romfs'])[0]!=fingerprint:raise ValueError('打包期间 RomFS 来源改变，未发布数据包。')
                    (stage/'.source.json').write_text(json.dumps(dict(fingerprint=fingerprint,files=n,parts=parts)))
                    (stage/'acnh-pack.json').unlink()
                    stage.rename(target)
                else:
                    print('复用已有数据包，校验中…',flush=True);verify_data(target)
                print('数据包：'+str(target),flush=True)
            name='ACNHMobile-'+('shell-' if kind=='shell' else '')+'v'+version['version']+'-build'+str(version['build'])+'-unsigned.ipa'
            dest=output/name
            # Keep a previously successful output recoverable until a verified replacement is ready.
            if dest.exists():trash_file(dest)
            made.rename(dest);dest.with_suffix('.ipa.sha256').write_text(digest+'  '+dest.name+'\n')
            if kind=='shell':shutil.copy2(job/'shell-output/shell-audit.json',output/'shell-audit.json')
            retain_two(output,dest)
            print('\n完成：'+str(dest),flush=True)
            print('仅保留最近两份 IPA；数据和存档不会清理。',flush=True)
            print('未签名。完整包需整包签名并配合外置 .data 使用。' if kind=='private' else '这是无游戏空壳，不能直接游玩。',flush=True)

if __name__=='__main__':
    code=0
    try:main()
    except KeyboardInterrupt:print('\n已取消，已有输出保留。',file=sys.stderr);code=130
    except (ValueError,RuntimeError,OSError,subprocess.CalledProcessError) as e:print('\n失败：'+str(e),file=sys.stderr,flush=True);code=1
    if sys.stdin.isatty() and '--plan' not in sys.argv:
        try:input('\n按回车关闭窗口…')
        except (EOFError,KeyboardInterrupt):pass
    sys.exit(code)
