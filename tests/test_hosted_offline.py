"""Offline orchestration must restore networking on every failure path."""
import importlib.util
from pathlib import Path
import unittest
import tempfile
import json
import io
from contextlib import redirect_stdout
from unittest.mock import patch
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location('hosted_offline',
    Path(__file__).resolve().parents[1] / 'scripts/ccode/hosted_offline.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class HostedOfflineTests(unittest.TestCase):
    def test_harness_diagnostics_only_emit_bounded_exact_stage_labels(self):
        output = io.StringIO()
        result = SimpleNamespace(returncode=1,
            stdout='private path\nOFFLINE HARNESS: platform\n'
                   'OFFLINE HARNESS: network-inventory\nOFFLINE HARNESS: private-secret\n'
                   'prefix OFFLINE HARNESS: cleanup\n', stderr='private credentials')
        with redirect_stdout(output):
            module.report_harness_progress(result)
        self.assertEqual(output.getvalue(),
                         'OFFLINE HARNESS: platform\nOFFLINE HARNESS: network-inventory\n')
        output = io.StringIO()
        result.stdout = 'OFFLINE HARNESS: setup\n' * 100
        with redirect_stdout(output):
            module.report_harness_progress(result)
        self.assertEqual(output.getvalue(), 'OFFLINE HARNESS: setup\n')

    def test_harness_error_location_is_numeric_bounded_and_message_free(self):
        output = io.StringIO()
        result = SimpleNamespace(stdout='OFFLINE HARNESS ERROR: line=247; category=7\n'
            'OFFLINE HARNESS ERROR: line=3; category=0\n'
            'OFFLINE HARNESS ERROR: line=99999999; category=7\n'
            'OFFLINE HARNESS ERROR: line=247; category=private\n'
            'OFFLINE HARNESS ERROR: line=247; category=7 secret\n', stderr='secret')
        with redirect_stdout(output):
            module.report_harness_progress(result)
        self.assertEqual(output.getvalue(), 'OFFLINE HARNESS ERROR: line=247; category=7\n')
        source = (Path(module.__file__).parent / 'accept-offline-windows11-x64.ps1').read_text()
        self.assertIn('$_.InvocationInfo.ScriptLineNumber', source)
        self.assertIn('[int]$_.CategoryInfo.Category', source)
        self.assertIn('    throw\n}', source)

    def test_harness_reports_stages_before_sensitive_operations(self):
        source = (Path(__file__).resolve().parents[1] /
                  'scripts/ccode/accept-offline-windows11-x64.ps1').read_text()
        for stage, operation in (
                ('setup', '$source = (Resolve-Path'),
                ('platform', '& $platformGate -Executable'),
                ('network-inventory', '$routeInventory = @('),
                ('baseline', '$serviceBefore = Get-ServiceInventory'),
                ('commands', '$version = Invoke-RecordedProcess'),
                ('post-inventory', '$programAfter = Get-DirectoryManifest'),
                ('evidence', '$evidence | ConvertTo-Json'),
                ('cleanup', 'if (-not $KeepWorkingDirectory)')):
            label = f'Write-Output "OFFLINE HARNESS: {stage}"'
            self.assertIn(label, source)
            self.assertLess(source.index(label), source.index(operation))
        controller = Path(module.__file__).read_text()
        self.assertIn('report_harness_progress(result)', controller)
        self.assertLess(controller.index('                report_harness_progress(result)'),
                        controller.index('                if result.returncode'))

    def test_phase_diagnostics_are_fixed_labels_and_never_report_failed_phase_passed(self):
        output = io.StringIO()
        with redirect_stdout(output):
            self.assertEqual(module.run_phase('adapter-discovery', lambda: 'private value'),
                             'private value')
        self.assertEqual(output.getvalue(),
                         'OFFLINE adapter-discovery: begin\nOFFLINE adapter-discovery: complete\n')
        output = io.StringIO()
        def fail():
            raise RuntimeError('private adapter and credentials')
        with redirect_stdout(output), self.assertRaises(RuntimeError):
            module.run_phase('disconnect', fail)
        self.assertEqual(output.getvalue(), 'OFFLINE disconnect: begin\n')
        output = io.StringIO()
        with patch.object(module, 'network') as action:
            with redirect_stdout(output), self.assertRaises(ValueError):
                module.run_phase('private injected label', action)
            action.assert_not_called()
        self.assertEqual(output.getvalue(), '')

    def test_execute_locates_bad_adapter_inventory_without_disclosing_it(self):
        output = io.StringIO()
        with patch.object(module.sys, 'platform', 'win32'), patch.dict(module.os.environ,
                GITHUB_ACTIONS='true', RUNNER_ENVIRONMENT='github-hosted'), \
                patch.object(module, 'powershell', side_effect=['', '["private-adapter"]']), \
                patch.object(module, 'network') as network, redirect_stdout(output):
            with self.assertRaises(ValueError):
                module.execute(SimpleNamespace(recover=None))
            network.assert_not_called()
        self.assertEqual(output.getvalue().splitlines(), [
            'OFFLINE host-check: begin', 'OFFLINE host-check: complete',
            'OFFLINE admin-check: begin', 'OFFLINE admin-check: complete',
            'OFFLINE adapter-discovery: begin', 'OFFLINE adapter-discovery: complete',
            'OFFLINE adapter-validation: begin'])

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
        self.assertEqual(module.adapter_ids(['{' + value.upper() + '}']), [value])
        for values in ([], ['private;command'], [value, value], [value, '{' + value + '}'],
                       ['{{' + value + '}}'], [value.replace('-', '')], [None], 'not-a-list'):
            with self.assertRaises(ValueError):
                module.adapter_ids(values)

    def test_network_selection_normalizes_windows_guid_for_disable_and_restore(self):
        value = '12345678-1234-1234-1234-123456789abc'
        for enable in (False, True):
            with patch.object(module, 'powershell') as invoke:
                module.network([value], enable)
            script = invoke.call_args.args[0]
            self.assertIn("([guid]$_.InterfaceGuid).ToString('D').ToLowerInvariant()", script)
            self.assertNotIn('$_.InterfaceGuid.ToString()', script)
            self.assertIn('if ($adapters.Count -ne $ids.Count)', script)

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
