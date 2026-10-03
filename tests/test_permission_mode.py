"""The real permission argument builder must preserve explicit modes and approvals."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PermissionModeTests(unittest.TestCase):
    def test_launcher_uses_permission_policy(self):
        source = (ROOT / 'scripts/ccode/launcher.cpp').read_text()
        self.assertIn('ccode::PermissionArguments(options.engine)', source)
        self.assertIn('--permission-prompt-tool mcp__ccode_permissions__approve', source)

    @unittest.skipUnless(shutil.which('clang++'), 'requires a native C++ compiler')
    def test_native_permission_policy_and_rpc(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'permission-tests'
            subprocess.run(['clang++', '-std=c++17',
                            str(ROOT / 'tests/ccode/permission-tests.cpp'),
                            '-o', str(binary)], check=True, capture_output=True)
            subprocess.run([str(binary)], check=True, capture_output=True)
