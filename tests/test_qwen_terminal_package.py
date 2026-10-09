"""Packaging gates. Windows also checks the embedded archive and real GUI handoff."""
from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]


class TerminalPackageTests(unittest.TestCase):
    def test_terminal_download_is_pinned_and_fails_closed_on_checksum(self):
        script = (ROOT / 'scripts/qwen/download-terminal.ps1').read_text()
        lock = json.loads((ROOT / 'scripts/qwen/terminal-release.json').read_text())
        self.assertEqual(lock['version'], '1.25.2733.0')
        self.assertRegex(lock['sha256'], r'^[0-9a-f]{64}$')
        self.assertIn('Get-FileHash', script)
        self.assertIn('throw "Windows Terminal SHA256 mismatch"', script)
        self.assertIn('https://github.com/microsoft/terminal/releases/download/', script)
        self.assertIn('ZipFile]::CreateFromDirectory', script)
        self.assertNotIn('Compress-Archive', script)  # It skips the hidden .portable marker.
        self.assertIn('NOTICE.html', script)
        self.assertIn('terminal-LICENSE', script)


if __name__ == '__main__':
    unittest.main()
