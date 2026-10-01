"""Portable snapshot evidence follows the exact operational lock scope."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'portable_fixture', Path(__file__).parent / 'ccode/portable-integration.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class PortableFixtureTests(unittest.TestCase):
    def test_snapshot_bytes_exclude_only_root_operational_locks(self):
        with tempfile.TemporaryDirectory() as folder:
            profile = Path(folder)
            (profile / 'history').mkdir()
            for name in ('frontend.lock', 'metadata.lock', 'history/frontend.lock',
                         'history/metadata.lock', 'history/session.jsonl'):
                (profile / name).write_bytes(b'exact')
            self.assertEqual(fixture.profile_bytes(profile), {
                'history/frontend.lock': b'exact',
                'history/metadata.lock': b'exact',
                'history/session.jsonl': b'exact',
            })
