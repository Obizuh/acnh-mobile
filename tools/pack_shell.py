#!/usr/bin/env python3
"""Add locally prepared owned runtime files to an ACNH shell. Never upload the resulting IPA."""
import sys
sys.dont_write_bytecode = True
import argparse, importlib.util, json, os, plistlib, re, shutil, tempfile, zipfile
from pathlib import Path, PurePosixPath
from build_shell import source_id, ROOT

def extract_shell(archive, dest):
    with zipfile.ZipFile(archive) as z:
        seen=set()
        for i in z.infolist():
            p=PurePosixPath(i.filename)
            if p.is_absolute() or '..' in p.parts or '\\' in i.filename or i.filename in seen or (i.external_attr>>16)&0o170000==0o120000:
                raise ValueError('Unsafe ZIP entry: '+i.filename)
            if not i.filename.startswith('Payload/ACNHMobile.app/'):
                raise ValueError('Not an ACNH shell archive')
            seen.add(i.filename)
        z.extractall(dest)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ['shell','patched','shaders','font','output']:
        ap.add_argument('--'+name,type=Path,required=True)
    a=ap.parse_args(); a.output=a.output.resolve()
    if a.output.exists(): raise ValueError('Output already exists')
    profile=ROOT/'dm/games/01006F8002326000'
    # Check everything before creating a large temporary install package.
    chain=re.search(r'#define DMSH_CHAIN "([0-9a-f]+)"',(profile/'dmsh_chain.h').read_text()).group(1)
    cache=a.shaders/('b-'+chain)
    libs=list(cache.glob('*_b.metallib'))
    if not libs: raise ValueError('Missing prepared iOS shader libraries: '+str(cache))
    for lib in libs:
        reflection=json.loads(lib.with_suffix('.json').read_text())
        if reflection.get('chain')!=chain: raise ValueError('Shader chain mismatch: '+lib.name)
    from fontTools.ttLib import TTFont
    with TTFont(a.font): pass
    for name in ['main','subsdk0','sdk']:
        for suffix in ['patched.bin','fixups.tsv','gates.tsv','cuts.tsv','obs.tsv']:
            if not (a.patched/(name+'.'+suffix)).is_file(): raise ValueError('Missing rewritten module table: '+name+'.'+suffix)
    with tempfile.TemporaryDirectory(prefix='acnh-pack-') as tmp:
        work=Path(tmp); extract_shell(a.shell,work)
        app=work/'Payload/ACNHMobile.app'; dm=app/'dm'
        metadata=json.loads((dm/'shell_info.json').read_text())
        if metadata.get('format')!=1 or metadata.get('source_id')!=source_id(ROOT):
            raise ValueError('Shell and source version differ; use the same release source')
        if (app/'Frameworks').exists() or (app/'msl').exists(): raise ValueError('Input is not an empty shell')
        spec=importlib.util.spec_from_file_location('guest',ROOT/'dm/tools/guest_dylib.py'); guest=importlib.util.module_from_spec(spec); spec.loader.exec_module(guest)
        guest.write(str(a.patched),str(app/'Frameworks'),metadata['modules'],'org.acnhmobile.experimental.guest')
        for name in metadata['modules']:
            for suffix in ['fixups.tsv','gates.tsv','cuts.tsv','obs.tsv']:
                shutil.copyfile(a.patched/(name+'.'+suffix),dm/(name+'.'+suffix))
        for name in ['shader_map.tsv','pso_manifest.tsv','c1_full.tsv','cb_extent.tsv']: shutil.copyfile(profile/name,dm/name)
        target=app/'msl'/('b-'+chain); target.mkdir(parents=True)
        for p in cache.iterdir():
            if p.is_file() and p.suffix in {'.metallib','.json'}: shutil.copyfile(p,target/p.name)
        (dm/'font').mkdir(); shutil.copyfile(a.font,dm/'font/standard.ttf')
        import subprocess,sys
        subprocess.run([sys.executable,ROOT/'dm/tools/make_ext_font.py',dm/'font/nintendo_ext.ttf'],check=True)
        with (dm/'env.txt').open('a') as f: f.write('\nDM_SHELL_CHAIN='+chain+'\n')
        a.output.parent.mkdir(parents=True,exist_ok=True)
        with zipfile.ZipFile(a.output,'x',zipfile.ZIP_DEFLATED) as z:
            for p in sorted((work/'Payload').rglob('*')):
                if p.is_file():z.write(p,p.relative_to(work))
    print('Private unsigned IPA:',a.output)
    print('Sign the entire IPA, including Guest.framework. Keep your .data files in Documents/ACNHMobile. Do not publish this IPA.')

if __name__=='__main__': main()
