#!/usr/bin/env python3
"""Compile ACNH UI shaders with NVN alpha-test state at fragment buffer 30."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
KEYS=('1bbd8e6baf1769fe_b','9aaab9f97ef18924_b','5c518b71b8502c40_b')
def patch(source):
    field=re.search(r'float4\s+(\w+)\s*\[\[color\(0\)\]\]',source)
    if not field or 'acnh_alpha [[buffer(30)]]'in source:raise ValueError('Unexpected or already patched shader')
    old='constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(0)]])'
    if old not in source:raise ValueError('Unexpected fragment signature')
    source=source.replace(old,old[:-1]+', constant float2& acnh_alpha [[buffer(30)]])')
    code='''    // NVN comparison happens on fragment alpha before blending or depth writes.
    float acnh_a = out.FIELD.w, acnh_r = acnh_alpha.y;
    uint acnh_f = uint(acnh_alpha.x);
    bool acnh_keep = acnh_f == 0u || acnh_f == 8u ||
        (acnh_f == 2u && acnh_a < acnh_r) ||
        (acnh_f == 3u && acnh_a == acnh_r) ||
        (acnh_f == 4u && acnh_a <= acnh_r) ||
        (acnh_f == 5u && acnh_a > acnh_r) ||
        (acnh_f == 6u && acnh_a != acnh_r) ||
        (acnh_f == 7u && acnh_a >= acnh_r);
    if (!acnh_keep) discard_fragment();
'''.replace('FIELD',field.group(1))
    if source.count('    return out;')!=1:raise ValueError('Unexpected fragment return')
    return source.replace('    return out;',code+'    return out;')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--cache',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();a.output.mkdir(parents=True,exist_ok=True);manifest={}
    for key in KEYS:
        source=patch((a.cache/(key+'.metal')).read_text());p=a.output/(key+'.metal');p.write_text(source)
        j=json.loads((a.cache/(key+'.json')).read_text());j.update(acnh_alpha_test=True,acnh_alpha_test_buffer=30);(a.output/(key+'.json')).write_text(json.dumps(j,ensure_ascii=False,indent=2));manifest[key]={}
        for platform,sdk in [('mac','macosx'),('ios','iphoneos')]:
            d=a.output/platform;d.mkdir(exist_ok=True);air=d/(key+'.air');lib=d/(key+'.metallib');cmd=['xcrun','-sdk',sdk,'metal','-c','-fmodules-cache-path='+str(a.output/'modulecache')]
            if platform=='ios':cmd+=['-mios-version-min=18.0']
            subprocess.run(cmd+[str(p),'-o',str(air)],check=True);subprocess.run(['xcrun','-sdk',sdk,'metallib',str(air),'-o',str(lib)],check=True);air.unlink();manifest[key][platform]=hashlib.sha256(lib.read_bytes()).hexdigest()
    (a.output/'manifest.json').write_text(json.dumps(manifest,indent=2));print(json.dumps(manifest,indent=2))
if __name__=='__main__':main()
