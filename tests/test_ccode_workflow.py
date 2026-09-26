"""Guard independent acceptance gates without adding CI Python dependencies."""
from pathlib import Path
import unittest


class AcceptanceWorkflowTests(unittest.TestCase):
    def test_portable_failure_does_not_hide_tool_lifecycle_acceptance(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text()
        step = workflow.split('      - name: Exercise actual engine tools through the frontend', 1)[1].split('      - ', 1)[0]
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", step)
        self.assertIn('python tests/ccode/tools-integration.py ./ccode.exe', step)
        self.assertNotIn('continue-on-error:', step)

    def test_workspace_alias_acceptance_is_independent_and_required(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text()
        step = workflow.split('      - name: Verify workspace aliases', 1)[1].split('      - ', 1)[0]
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", step)
        self.assertIn('python tests/ccode/tools-integration.py ./ccode.exe --workspace-aliases-only', step)
        self.assertNotIn('continue-on-error:', step)

    def test_payload_integrity_acceptance_is_independent_and_required(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text()
        step = workflow.split('      - name: Reject tampered embedded payload', 1)[1].split('      - ', 1)[0]
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", step)
        self.assertIn('python tests/ccode/payload-integrity.py ./ccode.exe', step)
        self.assertNotIn('continue-on-error:', step)

    def test_session_writer_concurrency_is_independent_and_required(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text()
        test_job = workflow.split('jobs:\n  test:\n', 1)[1].split('\n  long-workspace:', 1)[0]
        self.assertIn("version: ['2.1.221', '2.1.282']", test_job)
        step = test_job.split('      - name: Verify session writer concurrency', 1)[1].split('      - ', 1)[0]
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", step)
        self.assertIn('python tests/ccode/concurrency-integration.py ./ccode.exe', step)
        self.assertTrue((Path(__file__).parent / 'ccode/concurrency-integration.py').is_file())
        self.assertNotIn('continue-on-error:', step)

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
