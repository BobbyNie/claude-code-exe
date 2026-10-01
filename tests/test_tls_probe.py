"""The native TLS diagnostic probe must never publish raw event content."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('tls_probe', Path(__file__).parent / 'ccode/tls-probe.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class TlsProbeTests(unittest.TestCase):
    def test_event_summary_preserves_only_allowlisted_structural_evidence(self):
        events = [
            {'type': 'assistant', 'error': 'api_error', 'message': {'content': [
                {'type': 'text', 'text': 'private-token private-prompt'}]}},
            {'type': 'system', 'subtype': 'api_retry', 'error': 'private-token'},
            {'type': 'result', 'is_error': True, 'errors': ['private-token']},
            {'type': 'private-token', 'error': 'private-prompt'},
        ]
        self.assertEqual(probe.summarize_events(events), {
            'assistant_api_error': True, 'assistant_authentication_failed': False,
            'retry_event': True, 'error_result': True, 'structured_tls_code': False})

    def test_result_shape_summary_distinguishes_execution_failure_without_publishing_errors(self):
        events = [{'type': 'result', 'subtype': 'error_during_execution',
                   'errors': ['private-token private-prompt', {'code': 'private-code'}]},
                  {'type': 'assistant', 'subtype': 'error_max_turns'},
                  {'type': 'result', 'subtype': 'private-subtype', 'errors': 'private-error'}]
        self.assertEqual(probe.summarize_result_shape(events), {
            'execution_error': True, 'max_turns_error': False,
            'errors_array': True, 'string_error_entry': True, 'object_error_entry': True})
        self.assertFalse(any(probe.summarize_result_shape([{
            'type': 'assistant', 'errors': ['error_during_execution']}]).values()))

    def test_probe_refuses_wrong_size_or_non_windows_payload_metadata(self):
        import hashlib
        payload = b'MZ-fixture'
        metadata = {'engineSha256': hashlib.sha256(payload).hexdigest(),
                    'engineSize': len(payload), 'platform': 'windows', 'architecture': 'x64'}
        probe.verify_payload(payload, metadata)
        for field, value in [('engineSize', len(payload) + 1), ('platform', 'linux'),
                             ('architecture', 'arm64'), ('engineSha256', '0' * 64)]:
            with self.assertRaises(ValueError):
                probe.verify_payload(payload, dict(metadata, **{field: value}))

    def test_probe_environment_isolates_storage_and_enforces_no_fallback(self):
        root = Path('isolated')
        env = probe.probe_environment({'SystemRoot': 'system', 'PATH': 'path',
            'ANTHROPIC_API_KEY': 'private', 'APPDATA': 'private-home',
            'NODE_TLS_REJECT_UNAUTHORIZED': '0', 'HTTPS_PROXY': 'private-proxy'}, root)
        self.assertNotIn('private', str(env))
        self.assertNotIn('NODE_TLS_REJECT_UNAUTHORIZED', env)
        self.assertNotIn('HTTPS_PROXY', env)
        self.assertEqual(env['APPDATA'], str(root / 'roaming'))
        self.assertEqual(env['TEMP'], str(root / 'temp'))
        self.assertEqual(env['CLAUDE_CODE_MAX_RETRIES'], '0')
        self.assertEqual(env['CLAUDE_CODE_RETRY_WATCHDOG'], '0')
        self.assertEqual(env['CLAUDE_CODE_DISABLE_NONSTREAMING_FALLBACK'], '1')

    def test_tls_code_requires_structured_error_code_not_content(self):
        self.assertFalse(probe.summarize_events([{'type': 'assistant', 'message': {
            'content': [{'type': 'text', 'text': 'CERT_HAS_EXPIRED'}]}}])['structured_tls_code'])
        self.assertTrue(probe.summarize_events([{'type': 'system', 'subtype': 'api_retry',
            'error': {'code': 'CERT_HAS_EXPIRED'}}])['structured_tls_code'])
