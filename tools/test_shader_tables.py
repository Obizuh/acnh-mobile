"""Prepared-input table recovery and cache keys, without owned game files."""
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import shader_tables as s
import prepare_game


class Tables(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.profile = self.root / 'dm/games' / s.TITLE
        self.profile.mkdir(parents=True)
        self.ios = self.root / 'shader/ios'; self.chain = '0123456789ab'
        self.mac = self.root / 'shader/cache' / ('b-' + self.chain)
        self.mac.mkdir(parents=True); (self.mac / 'fixture_b.metal').write_text('// fixture')
        (self.profile / 'shader_map.tsv').write_text('x.bin\t0\tfragment\tfixture\t/source\n')
        (self.mac / 'fixture_b.metallib').write_bytes(b'fixture')
        (self.mac / 'fixture_b.json').write_text('{"chain":"'+self.chain+'"}')
        self.ran = []

    def generate(self, cmd, **kwargs):
        self.ran.append(Path(cmd[1]).name)
        Path(cmd[-1]).write_text('# chain b-' + self.chain + '\n')

    def test_missing_then_current(self):
        with patch.object(s.subprocess, 'run', self.generate):
            s.ensure_tables(self.root, self.ios, self.chain)
            self.assertEqual(self.ran, ['c1_guard.py', 'cb_extent.py'])
            s.ensure_tables(self.root, self.ios, self.chain)
            self.assertEqual(len(self.ran), 2)

    def test_only_stale_regenerated(self):
        (self.profile / 'c1_full.tsv').write_text('# chain b-' + self.chain + '\n')
        (self.profile / 'cb_extent.tsv').write_text('# chain b-old\n')
        with patch.object(s.subprocess, 'run', self.generate):
            s.ensure_tables(self.root, self.ios, self.chain)
        self.assertEqual(self.ran, ['cb_extent.py'])

    def test_no_mac_outputs_stops(self):
        (self.mac / 'fixture_b.metal').unlink()
        with self.assertRaisesRegex(ValueError, 'mac-shaders'):
            s.ensure_tables(self.root, self.ios, self.chain)

    def test_explicit_mac_path(self):
        self.ios = self.root / 'elsewhere/ios'
        with patch.object(s.subprocess, 'run', self.generate):
            s.ensure_tables(self.root, self.ios, self.chain, self.mac.parent)
        self.assertEqual(len(self.ran), 2)

    def test_incomplete_mac_does_not_generate_tables(self):
        (self.mac / 'fixture_b.metallib').unlink()
        with patch.object(s.subprocess, 'run', self.generate):
            with self.assertRaisesRegex(ValueError, '不完整'):
                s.ensure_tables(self.root, self.ios, self.chain)
        self.assertEqual(self.ran, [])

    def test_generated_tables_not_preparation_inputs(self):
        base, update = self.root / 'base.nsp', self.root / 'update.nsp'
        base.write_text('fixture'); update.write_text('fixture')
        family = {'base': base, 'updates': [(2228224, update)]}
        with patch.object(prepare_game, 'ROOT', self.root):
            before = prepare_game.cache_key(family)
            (self.profile / 'c1_full.tsv').write_text('new generated table')
            (self.profile / 'cb_extent.tsv').write_text('new generated table')
            self.assertEqual(prepare_game.cache_key(family), before)
            tool = self.root / 'tools/pipeline.py'; tool.parent.mkdir()
            tool.write_text('# updated implementation')
            self.assertNotEqual(prepare_game.cache_key(family), before)


if __name__ == '__main__':
    unittest.main()
