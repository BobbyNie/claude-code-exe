"""Check that the tamper fixture changes payload bytes, not launcher/metadata."""
import hashlib
import importlib.util
from pathlib import Path
import unittest
import urllib.request
import urllib.error


class PayloadIntegrityFixtureTests(unittest.TestCase):
    def test_activation_snapshot_distinguishes_intact_candidate_from_changed_cache(self):
        import tempfile
        path = Path(__file__).parent / 'ccode/payload-integrity.py'
        spec = importlib.util.spec_from_file_location('payload_integrity', path)
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        with tempfile.TemporaryDirectory(prefix='private-runtime-') as folder:
            root = Path(folder)
            original = b'MZ-original-fixture'
            (root / 'engine.new').write_bytes(original)
            (root / 'engine.exe').write_bytes(b'MZ-changed-fixture')
            snapshot = fixture.activation_snapshot(root / 'engine.exe',
                hashlib.sha256(original).hexdigest(), len(original))
            self.assertEqual(snapshot, {
                'cache': {'present': True, 'readable': True, 'size_matches': False,
                          'hash_matches': False, 'readonly': False},
                'candidate': {'present': True, 'readable': True, 'size_matches': True,
                              'hash_matches': True, 'readonly': False}})
            self.assertNotIn(folder, str(snapshot))
            (root / 'engine.new').unlink()
            self.assertEqual(fixture.activation_snapshot(root / 'engine.exe',
                hashlib.sha256(original).hexdigest(), len(original))['candidate'],
                dict.fromkeys(('present', 'readable', 'size_matches', 'hash_matches', 'readonly'), False))

    def test_failure_summary_is_exact_allowlisted_and_never_copies_private_output(self):
        path = Path(__file__).parent / 'ccode/payload-integrity.py'
        spec = importlib.util.spec_from_file_location('payload_integrity', path)
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        private = b'private-token E_GATEWAY_AUTH C:/private/workspace'
        summary = fixture.failure_summary(1, 0, private + b'\n[E_ENGINE: turn failed]\n', 1)
        self.assertEqual(summary, {'startup_code': None, 'attempt': 1, 'exit_code': 1, 'request_count': 0,
                                  'auth': False, 'engine': True, 'retry': False})
        self.assertNotIn('private', str(summary))
        self.assertTrue(fixture.failure_summary(1, 1,
            b'[E_GATEWAY_AUTH: authentication failed]\n', 0)['auth'])
        self.assertFalse(fixture.failure_summary(1, 1,
            b'prefix [E_GATEWAY_AUTH: authentication failed] suffix\n', 0)['auth'])

    def test_startup_failure_summary_only_accepts_exact_known_codes(self):
        path = Path(__file__).parent / 'ccode/payload-integrity.py'
        spec = importlib.util.spec_from_file_location('payload_integrity', path)
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        for code in ('E_EXTRACT', 'E_RUNTIME_BUSY', 'E_RUNTIME_PATH', 'E_CHECKSUM',
                     'E_EXTRACT_WRITE', 'E_EXTRACT_ACTIVATE', 'E_EXTRACT_ACCESS',
                     'E_EXTRACT_SHARING', 'E_EXTRACT_LOCKED', 'E_EXTRACT_DISK_FULL'):
            summary = fixture.failure_summary(64, 0, code.encode() + b'\r\n', 1)
            self.assertEqual(summary['startup_code'], code)
        for terminal in (b'private-token E_EXTRACT', b'E_EXTRACT C:/private',
                         b'E_UNKNOWN', b'E_EXTRACT\nE_RUNTIME_PATH\n'):
            self.assertIsNone(fixture.failure_summary(64, 0, terminal, 1)['startup_code'])
        self.assertIsNone(fixture.failure_summary(1, 0, b'E_EXTRACT\n', 1)['startup_code'])

    def test_only_a_verified_unique_payload_is_modified(self):
        path = Path(__file__).parent / 'ccode/payload-integrity.py'
        spec = importlib.util.spec_from_file_location('payload_integrity', path)
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        payload = b'MZ' + bytes(range(128))
        digest = hashlib.sha256(payload).hexdigest()
        original = b'launcher-header' + payload + b'original-metadata'
        changed = fixture.tamper_payload(original, payload, digest)
        differences = [i for i, pair in enumerate(zip(original, changed)) if pair[0] != pair[1]]
        self.assertEqual(len(original), len(changed))
        self.assertEqual(differences, [len(b'launcher-header') + len(payload) - 1])
        for candidate, expected in ((b'absent', digest), (original + payload, digest),
                                    (original, '0' * 64)):
            with self.assertRaises(ValueError):
                fixture.tamper_payload(candidate, payload, expected)

    def test_auth_fixture_counts_query_variants_without_hiding_duplicate_requests(self):
        path = Path(__file__).parent / 'ccode/payload-integrity.py'
        spec = importlib.util.spec_from_file_location('payload_integrity', path)
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        with fixture.rejecting_gateway() as server:
            for suffix in ('?beta=true', '?beta=false'):
                request = urllib.request.Request(server.url + '/v1/messages' + suffix,
                                                 data=b'{}')
                with self.assertRaises(urllib.error.HTTPError) as error:
                    urllib.request.urlopen(request, timeout=5)
                error.exception.close()
            self.assertEqual(server.requests, ['/v1/messages', '/v1/messages'])

    def test_auth_rejection_fixture_observes_real_http_without_retaining_secrets(self):
        path = Path(__file__).parent / 'ccode/payload-integrity.py'
        spec = importlib.util.spec_from_file_location('payload_integrity', path)
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        with fixture.rejecting_gateway() as server:
            request = urllib.request.Request(server.url + '/v1/messages', data=b'{"private":"not retained"}',
                                             headers={'Authorization': 'Bearer dummy-secret'})
            with self.assertRaises(urllib.error.HTTPError) as error:
                urllib.request.urlopen(request, timeout=5)
            self.assertEqual(error.exception.code, 401)
            self.assertEqual(server.requests, ['/v1/messages'])
            try:
                self.assertNotIn(b'private', error.exception.read())
            finally:
                error.exception.close()
