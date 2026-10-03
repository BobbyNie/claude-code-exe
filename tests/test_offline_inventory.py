"""Run real PowerShell helper contracts without modifying network adapters."""
from pathlib import Path
import shutil
import subprocess
import unittest


class OfflineInventoryTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('pwsh'), 'PowerShell is required')
    def test_real_inventory_hash_helper_handles_empty_display_fields(self):
        test = Path(__file__).parent / 'ccode/offline-inventory-tests.ps1'
        result = subprocess.run(['pwsh', '-NoProfile', '-NonInteractive', '-File', str(test)],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), 'PASS: offline inventory field hashing')
