"""Assembly must read the verified file object, not a swapped pathname."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts/ccode'))
spec = importlib.util.spec_from_file_location('snapshot_builder', ROOT / 'scripts/ccode/build_enterprise_package.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

class PackageInputSnapshotTests(unittest.TestCase):
    def test_replaced_path_between_observation_and_open_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'input'
            replacement = Path(directory) / 'replacement'
            source.write_bytes(b'approved bytes')
            replacement.write_bytes(b'unapproved bytes')
            original_lstat = Path.lstat
            swapped = False
            def observe(path, *args, **kwargs):
                nonlocal swapped
                status = original_lstat(path, *args, **kwargs)
                if path == source and not swapped:
                    swapped = True
                    replacement.replace(source)
                return status
            with patch.object(Path, 'lstat', observe):
                with self.assertRaises(builder.PackageBuildError) as failure:
                    builder._regular_bytes(source)
            self.assertEqual(failure.exception.code, 'E_INPUT_TYPE')

    def test_same_object_mutation_before_open_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'input'
            source.write_bytes(b'approved')
            original_lstat = Path.lstat
            mutated = False
            def observe(path, *args, **kwargs):
                nonlocal mutated
                status = original_lstat(path, *args, **kwargs)
                if path == source and not mutated:
                    mutated = True
                    source.write_bytes(b'unapproved longer bytes')
                return status
            with patch.object(Path, 'lstat', observe):
                with self.assertRaises(builder.PackageBuildError) as failure:
                    builder._regular_bytes(source)
            self.assertEqual(failure.exception.code, 'E_INPUT_READ')

    def test_unchanged_regular_input_preserves_exact_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'input'
            source.write_bytes(b'\x00exact\r\nbytes\xff')
            self.assertEqual(builder._regular_bytes(source), b'\x00exact\r\nbytes\xff')
