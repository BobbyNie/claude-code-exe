"""Check that the rename acceptance oracle detects stale names and changed bytes."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'tools_integration', Path(__file__).parent / 'ccode/tools-integration.py')
tools = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tools)


class ToolRenameFixtureTests(unittest.TestCase):
    def test_rename_oracle_requires_missing_old_name_and_intact_new_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'original.txt'
            target = Path(directory) / 'renamed 中文.txt'
            source.write_text('marker-after\n')
            target.write_text('marker-renamed\n')
            with self.assertRaisesRegex(AssertionError, 'old name'):
                tools.verify_rename_files(source, target)
            source.unlink()
            target.write_text('damaged')
            with self.assertRaisesRegex(AssertionError, 'bytes'):
                tools.verify_rename_files(source, target)
            target.write_text('marker-renamed\n')
            tools.verify_rename_files(source, target)
            (target.parent / 'absent-rename-parent').mkdir()
            (target.parent / 'absent-rename-parent/file.txt').write_text('unexpected')
            with self.assertRaisesRegex(AssertionError, 'destination'):
                tools.verify_rename_files(source, target)
