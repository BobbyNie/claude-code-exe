"""Portable snapshot evidence follows the exact operational lock scope."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock

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

    def test_interruption_requires_live_unpublished_snapshot_and_reaps_launcher(self):
        with tempfile.TemporaryDirectory() as folder:
            snapshots = Path(folder)
            pending = snapshots / 'a2345678-1234-1234-1234-123456789abc.pending'
            (pending / 'profile').mkdir(parents=True)
            process = Mock()
            process.poll.return_value = None
            process.returncode = 1
            self.assertEqual(fixture.interrupt_snapshot(process, snapshots), pending)
            process.kill.assert_called_once_with()
            process.communicate.assert_called_once_with(timeout=15)
            (pending / 'manifest.json').write_bytes(b'committed')
            process = Mock()
            process.poll.return_value = 0
            with self.assertRaises(AssertionError):
                fixture.interrupt_snapshot(process, snapshots)
            process.kill.assert_not_called()

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
