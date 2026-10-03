"""Provenance must identify the actual accepted host, not relabel Server as Client."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest

spec = importlib.util.spec_from_file_location('package_provenance',
    Path(__file__).parent / 'ccode/package-manifest-integration.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class ProvenanceEnvironmentTests(unittest.TestCase):
    def test_hosted_server_records_actual_os_and_elevated_token(self):
        version = SimpleNamespace(major=10, minor=0, build=26100, product_type=3)
        result = fixture.environment_record(version, 'AMD64', 64, True, 'github-hosted')
        self.assertEqual(result, {'acceptanceTarget': 'GitHub hosted Windows x64',
            'osMajor': 10, 'osMinor': 0, 'osBuild': 26100, 'windowsProductType': 3,
            'architecture': 'x64', 'processBits': 64, 'administratorToken': True})
        with self.assertRaises(ValueError):
            fixture.environment_record(version, 'AMD64', 64, True, 'windows11-ordinary')

    def test_wrong_architecture_and_false_ordinary_account_claim_are_rejected(self):
        version = SimpleNamespace(major=10, minor=0, build=22631, product_type=1)
        result = fixture.environment_record(version, 'AMD64', 64, False, 'windows11-ordinary')
        self.assertEqual(result['acceptanceTarget'], 'Windows 11 x64 ordinary account')
        for machine, bits, admin, target in (
                ('ARM64', 64, False, 'github-hosted'), ('AMD64', 32, False, 'github-hosted'),
                ('AMD64', 64, True, 'windows11-ordinary'), ('AMD64', 64, False, 'unknown')):
            with self.assertRaises(ValueError):
                fixture.environment_record(version, machine, bits, admin, target)
