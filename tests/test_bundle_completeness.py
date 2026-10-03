from pathlib import Path
import shutil
import subprocess
import unittest


class BundleCompletenessTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('pwsh'), 'PowerShell is required')
    def test_existing_four_product_release_is_not_complete(self):
        test = Path(__file__).parent / 'ccode/bundle-completeness-tests.ps1'
        result = subprocess.run(['pwsh', '-NoProfile', '-File', str(test)],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('PASS: bundle completeness', result.stdout)

    @unittest.skipUnless(shutil.which('pwsh'), 'PowerShell is required')
    def test_publication_preserves_release_and_checks_uploaded_bytes(self):
        test = Path(__file__).parent / 'ccode/bundle-publication-tests.ps1'
        result = subprocess.run(['pwsh', '-NoProfile', '-File', str(test)],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('PASS: bundle publication', result.stdout)
