"""Build the native Mac development host using matching prepared ACNH inputs."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
from prepare_game import load_pipeline
from shader_tables import chain_header, validate_cache, ensure_tables, TITLE


def build(root, patched, romfs, shaders, font, work, launch=False):
    root, patched, romfs, shaders, font, work = map(lambda p: Path(p).resolve(),
                                                   (root, patched, romfs, shaders, font, work))
    chain = chain_header(root)
    validate_cache(root, shaders, chain)
    ensure_tables(root, shaders, chain, shaders)
    if not font.is_file():
        raise ValueError('Missing standard font')
    for module in ('main', 'subsdk0', 'sdk'):
        if not (patched / (module + '.patched.bin')).is_file():
            raise ValueError('Missing rewritten module: ' + module)
    work.mkdir(parents=True, exist_ok=True)
    logs = work / 'logs'; logs.mkdir(exist_ok=True)
    sys.path.insert(0, str(root / 'dm/games'))
    import game
    g = game.load(TITLE)
    pipeline = load_pipeline()
    env = dict(os.environ, DM_GAME=TITLE)
    for key in ('DM_GAMES_DIR', 'DM_GAMES_FIRST', 'IOSOUT'):
        env.pop(key, None)
    command = pipeline.mac_run(sys.executable, root, work, romfs.parent, patched, shaders,
                               None, 'nvn', 'zh-Hans', env, logs, g)
    shutil.copyfile(font, work / 'font/standard.ttf')
    script = work / 'run-mac.command'
    script.write_text('#!/bin/bash\nset -e\n' + command + '\n')
    script.chmod(0o755)
    print('Mac development host: ' + str(work / 'tools/dmhost'))
    print('Run: ' + str(script))
    if launch:
        subprocess.run(['bash', str(script)], check=True)
    return script
