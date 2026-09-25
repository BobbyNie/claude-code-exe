"""Check that the tamper fixture changes payload bytes, not launcher/metadata."""
import hashlib
import importlib.util
from pathlib import Path
import unittest


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
