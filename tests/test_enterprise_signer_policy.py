"""Public RFC8032 engineering fixture only, never production signer approval."""
import hashlib
import os
import re
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / 'scripts/ccode/generate_enterprise_policy.py'
DER = bytes.fromhex('302a300506032b6570032100d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a')


class EnterpriseSignerPolicyTests(unittest.TestCase):
    def test_explicit_matching_pin_generates_fixed_native_policy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            key = root / 'public.der'
            key.write_bytes(DER)
            output = root / 'enterprise-policy.hpp'
            result = subprocess.run([sys.executable, str(GENERATOR), '--spki', str(key),
                '--approved-pin', hashlib.sha256(DER).hexdigest(), '--output', str(output)],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            policy = output.read_text(encoding='ascii')
            self.assertIn(hashlib.sha256(DER).hexdigest(), policy)
            self.assertEqual(bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', policy)), DER)
            self.assertIn('SignerPin', policy)
            self.assertEqual(result.stdout, '')

    def test_bad_pin_or_noncanonical_key_never_creates_output(self):
        for key_bytes, pin in [(DER, '0' * 64), (DER, hashlib.sha256(DER).hexdigest().upper()),
                               (DER + b'\0', hashlib.sha256(DER + b'\0').hexdigest()),
                               (b'X' + DER[1:], hashlib.sha256(b'X' + DER[1:]).hexdigest())]:
            with self.subTest(key_bytes=key_bytes, pin=pin), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                key = root / 'public.der'; key.write_bytes(key_bytes)
                output = root / 'policy.hpp'
                result = subprocess.run([sys.executable, str(GENERATOR), '--spki', str(key),
                    '--approved-pin', pin, '--output', str(output)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 1)
                self.assertEqual(result.stderr, 'E_SIGNER_POLICY\n')
                self.assertFalse(output.exists())

    def test_existing_output_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); key = root / 'public.der'; key.write_bytes(DER)
            output = root / 'policy.hpp'; output.write_bytes(b'sentinel')
            result = subprocess.run([sys.executable, str(GENERATOR), '--spki', str(key),
                '--approved-pin', hashlib.sha256(DER).hexdigest(), '--output', str(output)],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(output.read_bytes(), b'sentinel')

    def test_hardlinked_key_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); key = root / 'public.der'; key.write_bytes(DER)
            os.link(key, root / 'alias.der')
            output = root / 'policy.hpp'
            result = subprocess.run([sys.executable, str(GENERATOR), '--spki', str(key),
                '--approved-pin', hashlib.sha256(DER).hexdigest(), '--output', str(output)],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertFalse(output.exists())
