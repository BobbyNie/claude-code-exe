"""Guard independent acceptance gates without adding CI Python dependencies."""
from pathlib import Path
import unittest


class AcceptanceWorkflowTests(unittest.TestCase):
    def test_failed_gateway_does_not_hide_independent_acceptance(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text()
        upload = workflow.split('      - uses: actions/upload-artifact@v4', 1)[1].split('\n  long-workspace:', 1)[0]
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", upload)
        self.assertIn('name: ccode-windows-built-${{ matrix.version }}', upload)
        self.assertIn('if-no-files-found: error', upload)
        for name in ('long-workspace', 'cross-version'):
            job = workflow.split(f'\n  {name}:\n', 1)[1]
            header = job.split('    steps:', 1)[0]
            self.assertIn('needs: test', header)
            self.assertIn('if: ${{ !cancelled() }}', header)
        self.assertNotIn('ccode-windows-tested-', workflow)
        self.assertNotIn('continue-on-error:', workflow)


if __name__ == '__main__':
    unittest.main()
