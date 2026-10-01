"""Diagnostic children must not outlive their isolated temporary storage."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('tls_probe', Path(__file__).parent / 'ccode/tls-probe.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class ContainedProcessTests(unittest.TestCase):
    @unittest.skipIf(sys.platform == 'win32', 'Non-Windows refusal only')
    def test_non_windows_refuses_to_launch_uncontained_probe(self):
        with self.assertRaisesRegex(RuntimeError, 'Windows'):
            probe.run_contained([sys.executable, '-c', 'raise SystemExit(99)'],
                                cwd=Path.cwd(), env=os.environ, input='', timeout=1)

    @unittest.skipUnless(sys.platform == 'win32', 'Actual Windows Job Object required')
    def test_normal_parent_exit_reaps_descendant_holding_workspace(self):
        with tempfile.TemporaryDirectory() as folder:
            workspace = Path(folder) / 'workspace'
            workspace.mkdir()
            child = "import time; print('ready', flush=True); time.sleep(120)"
            parent = ('import subprocess,sys; '
                      f'p=subprocess.Popen([sys.executable,"-c",{child!r}],stdout=subprocess.PIPE); '
                      'assert p.stdout.readline()==b"ready\\n"; print("parent-done")')
            result = probe.run_contained([sys.executable, '-c', parent], cwd=workspace,
                                        env=os.environ, input='', timeout=15)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout.strip(), 'parent-done')
            workspace.rmdir()  # Windows refuses this while descendant cwd remains open.

    @unittest.skipUnless(sys.platform == 'win32', 'Actual Windows Job Object required')
    def test_timeout_reaps_parent_and_descendant_before_returning(self):
        with tempfile.TemporaryDirectory() as folder:
            workspace = Path(folder) / 'workspace'
            workspace.mkdir()
            child = "import pathlib,time; pathlib.Path('child-ready').write_text('ready'); time.sleep(120)"
            parent = ('import subprocess,sys,time; '
                      f'subprocess.Popen([sys.executable,"-c",{child!r}]); '
                      'time.sleep(120)')
            with self.assertRaises(subprocess.TimeoutExpired):
                probe.run_contained([sys.executable, '-c', parent], cwd=workspace,
                                    env=os.environ, input='', timeout=10)
            marker = workspace / 'child-ready'
            self.assertEqual(marker.read_text(), 'ready')
            marker.unlink()
            workspace.rmdir()
