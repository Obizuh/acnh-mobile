"""The rewriter's per-build patch files (dm/tools/sdk_patches.py, dm/games/<id>/patches.py, loaded by dm/tools/patch_nso.py): no build ID is claimed twice, every entry has
the form patch_nso.py expects, a game's file names only builds of its own profile's non-sdk modules, sdk_patches.py only sdk modules some profile ships; and the NSP script's
rewrite step keys its reuse on the patch files of the game's builds only. No game files needed (temporary files only in a temporary folder)."""
import importlib.util
from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'dm/tools')); sys.path.insert(0, str(REPO / 'dm/games'))
import game   # noqa: E402

FILES = [REPO / 'dm/tools/sdk_patches.py'] + sorted((REPO / 'dm/games').glob('*/patches.py'))


def load(path, n):
    spec = importlib.util.spec_from_file_location(f'patches_{n}', path); m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m


MODS = {p: load(p, n) for n, p in enumerate(FILES)}
HEX = re.compile(r'[0-9a-f]+$')


def builds_of(g):   # {build ID: module name} of a profile's build rows (a profile or its title ID)
    return {bid: m for row in (game.load(g) if isinstance(g, str) else g).builds().values() for m, bid in row.items() if m != 'display'}


class Registry(unittest.TestCase):
    def test_no_build_claimed_twice(self):
        for kind in ('CUTS', 'OBS', 'FRAME_STEP'):
            seen = {}
            for p, m in MODS.items():
                for bid in getattr(m, kind, {}):
                    self.assertNotIn(bid, seen, f'{kind} of {bid} in {seen.get(bid)} and {p}'); seen[bid] = p

    def test_entry_forms(self):
        for p, m in MODS.items():
            for bid, cuts in getattr(m, 'CUTS', {}).items():
                self.assertRegex(bid, r'^[0-9a-f]{32}$')
                for off, ob, nb, why in cuts:
                    self.assertTrue(isinstance(off, int) and HEX.match(ob) and HEX.match(nb) and len(ob) == len(nb) and len(ob) % 8 == 0 and why, f'{p} {bid} {off:#x}')
            for bid, gates in getattr(m, 'OBS', {}).items():
                for off, oid, word, why in gates:
                    self.assertTrue(isinstance(off, int) and isinstance(oid, int) and HEX.match(word) and len(word) == 8 and why, f'{p} {bid} {off:#x}')
            for bid, fn in getattr(m, 'FRAME_STEP', {}).items():
                self.assertTrue(callable(fn), f'{p} {bid}')
            self.assertFalse({k for k in vars(m) if k.isupper() and k not in ('CUTS', 'OBS', 'FRAME_STEP')} - set(dir(__import__('nso_asm'))),
                             f'{p}: a table patch_nso.py does not read')

    def test_game_files_name_their_own_builds(self):
        for p, m in MODS.items():
            if p.name != 'patches.py': continue
            own = builds_of(p.parent.name)
            for kind in ('CUTS', 'OBS', 'FRAME_STEP'):
                for bid in getattr(m, kind, {}):
                    self.assertIn(bid, own, f'{p}: {kind} of {bid}, which is no build of profile {p.parent.name}')
                    self.assertNotEqual(own[bid], 'sdk', f'{p}: {bid} is an sdk build; its patches go in dm/tools/sdk_patches.py')

    def test_sdk_file_does_not_claim_known_game_modules(self):
        # This source distribution contains ACNH only; omitted profiles cannot
        # classify the remaining upstream SDK builds. Check every known module.
        known = {bid: module for g in game.all_games() for bid, module in builds_of(g).items()}
        m = MODS[REPO / 'dm/tools/sdk_patches.py']
        self.assertFalse(getattr(m, 'OBS', {}) or getattr(m, 'FRAME_STEP', {}))
        for bid in m.CUTS:
            if bid in known: self.assertEqual(known[bid], 'sdk')


class RewriteKey(unittest.TestCase):
    def test_patch_files_of_the_games_builds(self):
        spec = importlib.util.spec_from_file_location('build_ipa_from_nsp', REPO / 'tools/upstream/nsp_pipeline.py')
        nsp = importlib.util.module_from_spec(spec); spec.loader.exec_module(nsp)
        tmp = Path(tempfile.mkdtemp()); self.addCleanup(shutil.rmtree, tmp)
        repo=tmp/'repo'; exe=tmp/'exefs';exe.mkdir()
        sdk=repo/'dm/tools/sdk_patches.py';sdk.parent.mkdir(parents=True)
        patch=repo/'dm/games/fixture/patches.py';patch.parent.mkdir(parents=True)
        sdk.write_text('# ' + '11'*16);patch.write_text('# ' + '22'*16)
        for name,bid in [('sdk','11'*16),('main','22'*16)]:
            (exe/name).write_bytes(b'NSO0'+bytes(0x3c)+bytes.fromhex(bid))
        self.assertEqual(nsp.patch_files(repo,exe,['main','sdk']),[sdk,patch])
        for name in ('main','sdk'):(exe/name).write_bytes(b'NSO0'+bytes(0x3c)+bytes(16))
        self.assertEqual(nsp.patch_files(repo,exe,['main','sdk']),[])
        self.assertNotIn(patch,nsp.profile_files(type('Profile',(),{'dir':patch.parent})()))



if __name__ == '__main__':
    unittest.main()
