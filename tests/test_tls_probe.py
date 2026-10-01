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

    def test_tls_code_requires_structured_error_code_not_content(self):
        self.assertFalse(probe.summarize_events([{'type': 'assistant', 'message': {
            'content': [{'type': 'text', 'text': 'CERT_HAS_EXPIRED'}]}}])['structured_tls_code'])
        self.assertTrue(probe.summarize_events([{'type': 'system', 'subtype': 'api_retry',
            'error': {'code': 'CERT_HAS_EXPIRED'}}])['structured_tls_code'])
