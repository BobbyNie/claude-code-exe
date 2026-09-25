"""Check that the tamper fixture changes payload bytes, not launcher/metadata."""
import hashlib
import importlib.util
from pathlib import Path
import unittest
import urllib.request
import urllib.error


class PayloadIntegrityFixtureTests(unittest.TestCase):
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
