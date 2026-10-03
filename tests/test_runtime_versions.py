"""Resolve one validated upstream snapshot, never hard-coded engine versions."""
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('runtime_versions', ROOT / 'scripts/ccode/runtime_versions.py')
versions = importlib.util.module_from_spec(spec)
spec.loader.exec_module(versions)


class RuntimeVersionTests(unittest.TestCase):
    def test_dynamic_channels_keep_real_cross_version_coverage(self):
        with patch.object(versions, 'fetch_version', side_effect=['3.4.8', '3.4.5']) as fetch:
            self.assertEqual(versions.resolve(), {
                'latest': '3.4.8', 'stable': '3.4.5', 'versions': ['3.4.5', '3.4.8'], 'cross_version': True})
            self.assertEqual([call.args[0] for call in fetch.call_args_list], ['latest', 'stable'])

    def test_equal_channels_build_once_without_fake_cross_version_pass(self):
        with patch.object(versions, 'fetch_version', return_value='3.4.8'):
            result = versions.resolve()
        self.assertEqual(result['versions'], ['3.4.8'])
        self.assertFalse(result['cross_version'])

    def test_invalid_or_reversed_channels_fail_closed(self):
        for latest, stable in [('oops', '3.4.5'), ('3.4.8\ninjected=1', '3.4.5'),
                               ('3.4.8', '3.4.9'), ('3.4.8-beta', '3.4.5')]:
            with self.subTest(latest=latest, stable=stable):
                with patch.object(versions, 'fetch_version', side_effect=[latest, stable]):
                    with self.assertRaises(ValueError):
                        versions.resolve()

    def test_both_workflows_consume_one_snapshot_and_skip_equal_cross_version(self):
        for name in ('test-ccode.yml', 'test-ccode-windows11-x64.yml'):
            text = (ROOT / '.github/workflows' / name).read_text(encoding='utf-8')
            self.assertIn('python scripts/ccode/runtime_versions.py', text)
            self.assertIn('version: ${{ fromJSON(needs.versions.outputs.versions) }}', text)
            self.assertIn("needs.versions.outputs.cross_version == 'true'", text)
            self.assertIn('needs.versions.outputs.stable', text)
            self.assertIn('needs.versions.outputs.latest', text)
            self.assertNotIn('2.1.221', text)
            self.assertNotIn('2.1.282', text)
