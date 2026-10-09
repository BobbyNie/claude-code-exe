"""Local release-contract checks; Windows executes the C# behavioral tests."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class QwenLauncherTests(unittest.TestCase):
    def test_direct_launcher_uses_portable_defaults_and_shared_console(self):
        source = (ROOT / 'scripts/qwen/launcher.cs').read_text()
        self.assertIn('CreateStartInfo(nodeExe, cliJs, args, exeDir)', source)
        self.assertIn('CreateNoWindow = false', source)
        self.assertIn('"QWEN_HOME"', source)
        self.assertIn('"QWEN_RUNTIME_DIR"', source)
        self.assertNotIn('RedirectStandardOutput = true', source)
        self.assertNotIn('RedirectStandardInput = true', source)

    def test_wrapper_preserves_explicit_data_paths(self):
        source = (ROOT / 'scripts/qwen/wrapper.bat').read_text()
        self.assertIn('if not defined QWEN_HOME set', source)
        self.assertIn('if not defined QWEN_RUNTIME_DIR set', source)

    def test_repaired_bundle_has_a_distinct_tag_and_windows_gate(self):
        versions = (ROOT / 'scripts/check-all-versions.ps1').read_text()
        workflow = (ROOT / '.github/workflows/auto-release.yml').read_text()
        self.assertIn('-qwen-launcher-r4"', versions)
        self.assertIn('tests/qwen/test-windows.ps1', workflow)
        self.assertIn('Test packaged Qwen startup', workflow)
        self.assertIn('Console.CancelKeyPress += handler', (ROOT / 'scripts/qwen/launcher.cs').read_text())
        self.assertIn('release\\qwen-launcher.revision -Value "4"', workflow)

    def test_windows_compiler_receives_resolved_native_source_paths(self):
        script = (ROOT / 'tests/qwen/test-windows.ps1').read_text()
        self.assertIn('(Resolve-Path -LiteralPath $LauncherSource).Path', script)
        self.assertIn("(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'launcher-tests.cs')).Path", script)


if __name__ == '__main__':
    unittest.main()
