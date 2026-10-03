"""Portable snapshot evidence follows the exact operational lock scope."""
import importlib.util
import json
import os
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location(
    'portable_fixture', Path(__file__).parent / 'ccode/portable-integration.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class PortableFixtureTests(unittest.TestCase):
    def test_cleanup_probe_is_private_and_preserves_original_failure(self):
        error = PermissionError('private-path-marker')
        with unittest.mock.patch.object(fixture, 'file_occupancy', return_value={
                'available': True, 'process_count': 2, 'test_process_present': False}, create=True):
            with unittest.mock.patch('builtins.print') as output:
                with self.assertRaises(PermissionError) as caught:
                    with fixture.diagnose_cleanup(Path('private-path-marker')):
                        raise error
                self.assertIs(caught.exception, error)
                self.assertNotIn('private-path-marker', str(output.call_args))
                self.assertIn('process_count', str(output.call_args))

    def test_interrupted_snapshot_cleanup_reports_occupancy_without_swallowing_failure(self):
        error = PermissionError('private-snapshot-marker')
        original_temporary_directory = tempfile.TemporaryDirectory
        class FailingCleanup(original_temporary_directory):
            def __exit__(self, *args):
                super().__exit__(*args)
                raise error
        with original_temporary_directory() as folder:
            executable = Path(folder) / 'fixture.exe'
            executable.write_bytes(b'fixture')
            process = Mock()
            process.poll.return_value = 0
            with unittest.mock.patch.object(fixture.tempfile, 'TemporaryDirectory', FailingCleanup), \
                    unittest.mock.patch.object(fixture.subprocess, 'Popen', return_value=process), \
                    unittest.mock.patch.object(fixture, 'interrupt_snapshot', side_effect=RuntimeError('stop fixture')), \
                    unittest.mock.patch.object(fixture, 'file_occupancy', return_value={
                        'available': True, 'process_count': 1, 'test_process_present': False}) as probe, \
                    unittest.mock.patch('builtins.print') as output:
                with self.assertRaises(PermissionError) as caught:
                    fixture.check_interrupted_snapshot(executable)
                self.assertIs(caught.exception, error)
                probe.assert_called_once()
                self.assertEqual(probe.call_args.args[0].name, 'ccode.exe')
                self.assertNotIn('private-snapshot-marker', str(output.call_args))
                self.assertIn('process_count', str(output.call_args))

    @unittest.skipUnless(os.name == 'nt', 'Restart Manager requires Windows')
    def test_restart_manager_observes_actual_test_executable_without_identifiers(self):
        observation = fixture.file_occupancy(Path(sys.executable))
        self.assertEqual(set(observation), {'available', 'process_count', 'test_process_present'})
        self.assertTrue(observation['available'])
        self.assertGreater(observation['process_count'], 0)
        self.assertTrue(observation['test_process_present'])

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

    def test_data_failure_command_requires_fresh_exact_report_and_neutral_terminal(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            def run(*args):
                self.assertIn('--diagnostics', args)
                path = Path(args[args.index('--diagnostics') + 1])
                self.assertFalse(path.exists())
                report = {'schemaVersion': 1, 'product': 'ccode', 'platform': 'windows',
                    'architecture': 'x64', 'status': 'error',
                    'operationId': 'a2345678-1234-4234-8234-123456789abc',
                    'errorCode': 'E_SESSION_DATA', 'category': 'data', 'exitCode': 64,
                    'privacy': dict.fromkeys(('argumentsCaptured', 'environmentValuesCaptured',
                        'promptOrContentCaptured', 'credentialsCaptured'), False)}
                path.write_text(json.dumps(report), encoding='utf-8')
                return SimpleNamespace(returncode=64, stdout='', stderr='E_SESSION_DATA\n')
            fixture.run_data_failure(run, root, 'E_SESSION_DATA', '--sessions')
            fixture.run_data_failure(run, root, 'E_SESSION_DATA', '--sessions')
            self.assertEqual(len(list(root.glob('*.json'))), 2)
            for output in (SimpleNamespace(returncode=64, stdout='', stderr='E_SESSION_DATA\n'),
                           SimpleNamespace(returncode=0, stdout='', stderr=''),
                           SimpleNamespace(returncode=64, stdout='private', stderr='E_SESSION_DATA')):
                with self.assertRaises(AssertionError):
                    fixture.run_data_failure(lambda *args: output, root, 'E_SESSION_DATA', '--sessions')

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
