"""Portable snapshot evidence follows the exact operational lock scope."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'portable_fixture', Path(__file__).parent / 'ccode/portable-integration.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class PortableFixtureTests(unittest.TestCase):
    def test_corrupt_pointer_report_requires_data_category_and_exact_privacy_schema(self):
        report = {'schemaVersion': 1, 'product': 'ccode', 'platform': 'windows',
            'architecture': 'x64', 'status': 'error',
            'operationId': 'a2345678-1234-4234-8234-123456789abc',
            'errorCode': 'E_ACTIVE_PROFILE', 'category': 'data', 'exitCode': 64,
            'privacy': dict.fromkeys(('argumentsCaptured', 'environmentValuesCaptured',
                'promptOrContentCaptured', 'credentialsCaptured'), False)}
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'failure.json'
            path.write_text(json.dumps(report), encoding='utf-8')
            fixture.validate_data_failure_report(path, 'E_ACTIVE_PROFILE', 64)
            for change in ({'category': 'local'}, {'privatePath': 'private-marker'}):
                path.write_text(json.dumps(dict(report, **change)), encoding='utf-8')
                with self.assertRaises(AssertionError):
                    fixture.validate_data_failure_report(path, 'E_ACTIVE_PROFILE', 64)

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
