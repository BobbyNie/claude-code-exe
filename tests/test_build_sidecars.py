"""Build-time boundary export does not need an unsigned enterprise launcher."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class BuildSidecarTests(unittest.TestCase):
    def test_build_exports_exact_embedded_provenance_and_independent_boundary(self):
        source = (ROOT / 'scripts/ccode/build.ps1').read_text()
        self.assertIn('Copy-Item -LiteralPath $metadataPath', source)
        self.assertIn('package-provenance.json', source)
        self.assertIn('runtime-boundary.json', source)
        self.assertIn('export-build-boundary.cpp', source)

    @unittest.skipUnless(shutil.which('clang++'), 'requires a native C++ compiler')
    def test_independent_boundary_export_is_valid_and_creates_no_runtime_data(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / 'export-boundary'
            subprocess.run(['clang++', '-std=c++17', str(ROOT / 'scripts/ccode/export-build-boundary.cpp'),
                            '-o', str(executable)], check=True, capture_output=True)
            result = subprocess.run([str(executable)], cwd=directory, check=True,
                                    capture_output=True, text=True)
            document = json.loads(result.stdout)
            self.assertEqual(document['minimumWindowsBuild'], 22000)
            self.assertEqual(document['architecture'], 'x64')
            self.assertFalse(document['sideEffects']['extractsRuntime'])
            self.assertEqual(list(Path(directory).iterdir()), [executable])
