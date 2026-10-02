"""Lifecycle JSON reads reject replaced file objects without leaking contents."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts/ccode'))
spec = importlib.util.spec_from_file_location('snapshot_lifecycle', ROOT / 'scripts/ccode/enterprise_lifecycle.py')
lifecycle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lifecycle)


class LifecycleInputSnapshotTests(unittest.TestCase):
    def test_json_replacement_after_initial_observation_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'audit.json'
            replacement = Path(directory) / 'replacement.json'
            source.write_bytes(b'{"status":"passed"}')
            replacement.write_bytes(b'{"private":"unapproved"}')
            original = Path.lstat
            swapped = False
            def observe(path, *args, **kwargs):
                nonlocal swapped
                status = original(path, *args, **kwargs)
                if path == source and not swapped:
                    swapped = True
                    replacement.replace(source)
                return status
            with patch.object(Path, 'lstat', observe):
                with self.assertRaises(lifecycle.LifecycleError) as failure:
                    lifecycle._json(source, 'E_LIFECYCLE_AUDIT')
            self.assertEqual(str(failure.exception), 'E_LIFECYCLE_AUDIT')
