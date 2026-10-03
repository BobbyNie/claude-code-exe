"""Offline orchestration must restore networking on every failure path."""
import importlib.util
from pathlib import Path
import unittest
import tempfile
import json
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('hosted_offline',
    Path(__file__).resolve().parents[1] / 'scripts/ccode/hosted_offline.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class HostedOfflineTests(unittest.TestCase):
    def test_success_disconnects_before_trial_and_restores_before_returning(self):
        events = []
        module.isolated_trial(lambda: events.append('disable'),
                              lambda: events.append('trial'),
                              lambda: events.append('restore'))
        self.assertEqual(events, ['disable', 'trial', 'restore'])

    def test_partial_disconnect_and_trial_failures_always_restore(self):
        for phase in ('disable', 'trial'):
            events = []
            def action(name):
                events.append(name)
                if name == phase:
                    raise RuntimeError('private network diagnostic')
            with self.assertRaisesRegex(RuntimeError, '^E_HOSTED_OFFLINE_TRIAL$'):
                module.isolated_trial(lambda: action('disable'), lambda: action('trial'),
                                      lambda: events.append('restore'))
            self.assertEqual(events[-1], 'restore')

    def test_restore_failure_is_never_ignored(self):
        def fail():
            raise RuntimeError('private adapter identity')
        for trial in (lambda: None, fail):
            with self.assertRaisesRegex(RuntimeError, '^E_HOSTED_OFFLINE_RESTORE$'):
                module.isolated_trial(lambda: None, trial, fail)

    def test_host_gate_rejects_local_or_non_windows_execution(self):
        for platform, env in [('darwin', {'GITHUB_ACTIONS': 'true', 'RUNNER_ENVIRONMENT': 'github-hosted'}),
                              ('win32', {}),
                              ('win32', {'GITHUB_ACTIONS': 'true', 'RUNNER_ENVIRONMENT': 'self-hosted'})]:
            with self.assertRaisesRegex(RuntimeError, 'E_HOSTED_OFFLINE_HOST'):
                module.require_host(platform, env)
        module.require_host('win32', {'GITHUB_ACTIONS': 'true', 'RUNNER_ENVIRONMENT': 'github-hosted'})

    def test_adapter_ids_must_be_unique_canonical_guids(self):
        value = '12345678-1234-1234-1234-123456789abc'
        self.assertEqual(module.adapter_ids([value]), [value])
        for values in ([], ['private;command'], [value, value], 'not-a-list'):
            with self.assertRaises(ValueError):
                module.adapter_ids(values)

    def test_recovery_waits_for_done_or_restores_on_explicit_failure_signal(self):
        value = '12345678-1234-1234-1234-123456789abc'
        for suffix, expected in (('.done', False), ('.restore-now', True)):
            with tempfile.TemporaryDirectory() as root:
                state = Path(root) / 'state.json'
                state.write_text(json.dumps({'adapters': [value]}), encoding='utf-8')
                state.with_suffix(suffix).touch()
                with patch.object(module, 'network') as network:
                    module.recover(state)
                self.assertEqual(network.called, expected)
                self.assertEqual(state.with_suffix('.recovered').exists(), expected)
                if expected:
                    network.assert_called_once_with([value], True)

    def test_hosted_workflow_requires_real_isolation_trial(self):
        workflow = (Path(__file__).resolve().parents[1] / '.github/workflows/test-ccode.yml').read_text(encoding='utf-8')
        self.assertIn('  offline-hosted:', workflow)
        step = workflow.split('  offline-hosted:', 1)[1].split('\n  cross-version:', 1)[0]
        self.assertIn('scripts/ccode/hosted_offline.py', step)
        self.assertIn('ccode-windows-built-${{ matrix.version }}', step)
        self.assertIn('if-no-files-found: error', step)
        self.assertNotIn('continue-on-error', step)
