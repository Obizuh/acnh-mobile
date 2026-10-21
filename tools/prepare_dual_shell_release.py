#!/usr/bin/env python3
"""Audit two independently built game-free shells and stage exactly three assets."""
import argparse, hashlib, json, plistlib, re, shutil, subprocess, tempfile, zipfile
from pathlib import Path, PurePosixPath
from build_shell import audit_app, source_id

EDITIONS = [('iOS26', '26.0'), ('iOS18-compat', '18.0')]

def inspect(archive, source, expected_minimum, version):
    with tempfile.TemporaryDirectory(prefix='acnh-shell-audit-') as tmp:
        root = Path(tmp)
        with zipfile.ZipFile(archive) as z:
            if z.testzip() is not None:
                raise ValueError('Corrupt shell ZIP')
            seen = set()
            for item in z.infolist():
                name = PurePosixPath(item.filename)
                if (name.is_absolute() or '..' in name.parts or '\\' in item.filename or
                    item.filename in seen or (item.external_attr >> 16) & 0o170000 == 0o120000 or
                    not item.filename.startswith('Payload/ACNHMobile.app/')):
                    raise ValueError('Unsafe shell ZIP entry: ' + item.filename)
                seen.add(item.filename)
            z.extractall(root)
        app = root / 'Payload/ACNHMobile.app'
        files = audit_app(app)  # Strict allowlist, icons, Mach-O game-section audit.
        info = plistlib.loads((app / 'Info.plist').read_bytes())
        meta = json.loads((app / 'dm/shell_info.json').read_text())
        if (info.get('CFBundleShortVersionString') != version['version'] or
            str(info.get('CFBundleVersion')) != str(version['build']) or
            info.get('MinimumOSVersion') != expected_minimum):
            raise ValueError('Shell Info.plist version or installation minimum mismatch')
        if (meta.get('format') != 1 or meta.get('game_content') is not False or
            meta.get('version') != version['version'] or meta.get('build') != version['build'] or
            meta.get('minimum_ios') != expected_minimum or meta.get('source_id') != source_id(source)):
            raise ValueError('Shell metadata/source fingerprint mismatch')
        load = subprocess.check_output(['otool', '-l', str(app / 'DmIOS')], text=True)
        if not re.search(r'\bminos\s+' + re.escape(expected_minimum) + r'\b', load):
            raise ValueError('Mach-O installation minimum mismatch')
        if expected_minimum == '18.0':
            imports = subprocess.check_output(['nm', '-u', str(app / 'DmIOS')], text=True)
            if re.search(r'\b_?OBJC_CLASS_\$_MTL4|\b_?MTL4', imports):
                raise ValueError('Compatibility shell imports Metal 4')
        return {'minimum_ios': expected_minimum, 'source_id': meta['source_id'],
                'files': files, 'game_content': False, 'zip_valid': True}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['main-source', 'compat-source', 'main-archive', 'compat-archive', 'output']:
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    sources = [a.main_source.resolve(), a.compat_source.resolve()]
    archives = [a.main_archive.resolve(), a.compat_archive.resolve()]
    version = json.loads((sources[0] / 'config/version.json').read_text())
    if json.loads((sources[1] / 'config/version.json').read_text()) != version:
        raise ValueError('Both editions must have identical version and build')
    if not re.fullmatch(r'[0-9]+(?:\.[0-9]+){2}', version['version']) or not isinstance(version['build'], int):
        raise ValueError('Invalid shared version')
    output = a.output.resolve()
    if output.exists():
        raise ValueError('Choose a new output directory; existing assets are never overwritten')
    reports = []
    # Audit both before staging either as a publishable asset.
    for (edition, minimum), archive, source in zip(EDITIONS, archives, sources):
        r = inspect(archive, source, minimum, version)
        r.update(edition=edition, archive=archive)
        reports.append(r)
    output.mkdir(parents=True)
    checksums = []
    for report in reports:
        name = 'ACNHMobile-' + report['edition'] + '-v' + version['version'] + '-build' + str(version['build']) + '-shell-unsigned.ipa'
        dest = output / name
        shutil.copyfile(report.pop('archive'), dest)
        report.update(name=name, bytes=dest.stat().st_size, sha256=hashlib.sha256(dest.read_bytes()).hexdigest())
        checksums.append(report['sha256'] + '  ' + name)
    (output / 'SHA256SUMS.txt').write_text('\n'.join(checksums) + '\n')
    # Keep audit metadata alongside CI artifacts, but do not attach it to Release.
    (output / 'audit-report.json').write_text(json.dumps({'version': version, 'editions': reports}, indent=2) + '\n')
    print(json.dumps({'version': version, 'assets': [{k: r[k] for k in ['name','bytes','sha256','minimum_ios','source_id']} for r in reports]}, indent=2))

if __name__ == '__main__':
    main()
