"""Guard independent acceptance gates without adding CI Python dependencies."""
from pathlib import Path
import unittest
import ast


class AcceptanceWorkflowTests(unittest.TestCase):
    def test_signature_contracts_have_pinned_runtime_and_required_gate(self):
        root = Path(__file__).resolve().parents[1]
        for name in ('test-ccode.yml', 'test-ccode-windows11-x64.yml'):
            workflow = (root / '.github/workflows' / name).read_text(encoding='utf-8')
            self.assertIn('actions/setup-node@820762786026740c76f36085b0efc47a31fe5020', workflow)
            self.assertIn("node-version: '22.23.3'", workflow)
            step = workflow.split('      - name: Test manifest signature contracts', 1)[1].split('      - ', 1)[0]
            self.assertIn('node --test tests/ccode/manifest-signature.test.mjs', step)
            self.assertIn("if ($LASTEXITCODE -ne 0)", step)
            self.assertNotIn('continue-on-error:', step)
            self.assertLess(workflow.index('actions/setup-node@'),
                            workflow.index('      - name: Test Python contracts'))

    def test_extension_gates_run_independently_after_other_test_failures(self):
        root = Path(__file__).resolve().parents[1]
        for name in ('test-ccode.yml', 'test-ccode-windows11-x64.yml'):
            workflow = (root / '.github/workflows' / name).read_text(encoding='utf-8')
            for title, option in (('Verify actual MCP allow and deny', '--mcp-only'),
                                  ('Verify actual Skill body loading', '--skill-only'),
                                  ('Verify actual subagent execution', '--subagent-only')):
                with self.subTest(workflow=name, gate=title):
                    self.assertIn('      - name: ' + title, workflow)
                    step = workflow.split('      - name: ' + title, 1)[1].split('      - ', 1)[0]
                    self.assertIn("!cancelled() && steps.build.outcome == 'success'", step)
                    if 'windows11' in name:
                        self.assertIn("steps.platform.outcome == 'success'", step)
                    self.assertIn('python tests/ccode/tools-integration.py ./ccode.exe ' + option, step)
                    self.assertNotIn('continue-on-error', step)

    def test_source_reads_explicitly_use_utf8_on_windows(self):
        source = Path(__file__).read_text(encoding='utf-8')
        reads = [node for node in ast.walk(ast.parse(source))
                 if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                 and node.func.attr == 'read_text']
        self.assertTrue(reads)
        for node in reads:
            self.assertTrue(any(keyword.arg == 'encoding' and
                                isinstance(keyword.value, ast.Constant) and
                                keyword.value.value == 'utf-8'
                                for keyword in node.keywords),
                            f'Source read at line {node.lineno} relies on Windows locale')

    def test_python_failure_does_not_hide_native_runtime_gate(self):
        root = Path(__file__).resolve().parents[1]
        for name in ('test-ccode.yml', 'test-ccode-windows11-x64.yml'):
            with self.subTest(workflow=name):
                workflow = (root / '.github/workflows' / name).read_text(encoding='utf-8')
                python_step = workflow.split('      - name: Test Python contracts', 1)[1].split('      - ', 1)[0]
                native_step = workflow.split('      - name: Test native runtime and resume', 1)[1].split('      - ', 1)[0]
                self.assertIn('python -m unittest discover -s tests -v', python_step)
                self.assertIn("throw 'Python tests failed'", python_step)
                self.assertNotIn('test-windows.ps1', python_step)
                self.assertIn('./scripts/ccode/test-windows.ps1 -Executable ./ccode.exe', native_step)
                self.assertNotIn('unittest', native_step)
                self.assertIn("!cancelled() && steps.build.outcome == 'success'", native_step)
                if 'windows11' in name:
                    self.assertIn("steps.platform.outcome == 'success'", native_step)
                self.assertNotIn('continue-on-error:', python_step + native_step)

    def test_portable_failure_does_not_hide_tool_lifecycle_acceptance(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text(encoding='utf-8')
        step = workflow.split('      - name: Exercise actual engine tools through the frontend', 1)[1].split('      - ', 1)[0]
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", step)
        self.assertIn('python tests/ccode/tools-integration.py ./ccode.exe', step)
        self.assertNotIn('continue-on-error:', step)

    def test_workspace_alias_acceptance_is_independent_and_required(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text(encoding='utf-8')
        step = workflow.split('      - name: Verify workspace aliases', 1)[1].split('      - ', 1)[0]
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", step)
        self.assertIn('python tests/ccode/tools-integration.py ./ccode.exe --workspace-aliases-only', step)
        self.assertNotIn('continue-on-error:', step)

    def test_workspace_boundary_acceptance_is_independent_and_required(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text(encoding='utf-8')
        job = workflow.split('\n  workspace-boundary:\n', 1)[1].split('\n  cross-version:', 1)[0]
        step = job.split('      - name: Verify explicit workspace cwd boundaries', 1)[1].split('      - ', 1)[0]
        self.assertIn('python tests/ccode/tools-integration.py ./workspace-boundary-bin/ccode.exe --workspace-boundary-only', step)
        self.assertNotIn('continue-on-error:', step)

    def test_payload_integrity_acceptance_is_independent_and_required(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text(encoding='utf-8')
        step = workflow.split('      - name: Reject tampered embedded payload', 1)[1].split('      - ', 1)[0]
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", step)
        self.assertIn('python tests/ccode/payload-integrity.py ./ccode.exe', step)
        self.assertNotIn('continue-on-error:', step)

    def test_session_writer_concurrency_is_independent_and_required(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text(encoding='utf-8')
        test_job = workflow.split('jobs:\n  test:\n', 1)[1].split('\n  workspace-boundary:', 1)[0]
        self.assertIn("version: ['2.1.221', '2.1.282']", test_job)
        step = test_job.split('      - name: Verify session writer concurrency', 1)[1].split('      - ', 1)[0]
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", step)
        self.assertIn('python tests/ccode/concurrency-integration.py ./ccode.exe', step)
        self.assertTrue((Path(__file__).parent / 'ccode/concurrency-integration.py').is_file())
        self.assertNotIn('continue-on-error:', step)

    def test_windows11_x64_acceptance_has_a_dedicated_ordinary_account_gate(self):
        root = Path(__file__).resolve().parents[1]
        workflow_path = root / '.github/workflows/test-ccode-windows11-x64.yml'
        self.assertTrue(workflow_path.is_file())
        workflow = workflow_path.read_text(encoding='utf-8')
        self.assertIn('workflow_dispatch:', workflow)
        self.assertIn('runs-on: [self-hosted, Windows, X64, windows-11]', workflow)
        self.assertIn("version: ['2.1.221', '2.1.282']", workflow)
        self.assertIn('Verify Windows 11 x64 ordinary-account environment', workflow)
        self.assertIn('./scripts/ccode/assert-windows11-x64.ps1', workflow)
        for command in (
            'python -m unittest discover -s tests -v',
            './scripts/ccode/test-windows.ps1 -Executable ./ccode.exe',
            'python tests/ccode/portable-integration.py ./ccode.exe',
            'python tests/ccode/tools-integration.py ./ccode.exe',
            'python tests/ccode/tools-integration.py ./ccode.exe --workspace-aliases-only',
            'python tests/ccode/tools-integration.py ./ccode.exe --workspace-boundary-only',
            'python tests/ccode/payload-integrity.py ./ccode.exe',
            'python tests/ccode/gateway-integration.py ./ccode.exe',
            'python tests/ccode/concurrency-integration.py ./ccode.exe',
        ):
            self.assertIn(command, workflow)
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", workflow)
        self.assertIn("needs.test.result == 'success'", workflow)
        self.assertNotIn('continue-on-error:', workflow)
        platform_script = root / 'scripts/ccode/assert-windows11-x64.ps1'
        self.assertTrue(platform_script.is_file())
        platform = platform_script.read_text(encoding='utf-8')
        for requirement in (
            'RuntimeInformation]::OSArchitecture',
            'Architecture]::X64',
            'CurrentBuildNumber',
            'InstallationType',
            'Windows 11',
            'WindowsBuiltInRole]::Administrator',
            "SecurityIdentifier]::new('S-1-5-32-544')",
            'administratorsMember',
            'ExpectedEngineVersion',
            'Get-FileHash',
            'ConvertTo-Json',
        ):
            self.assertIn(requirement, platform)

    def test_offline_windows11_acceptance_is_standalone_and_records_host_deltas(self):
        root = Path(__file__).resolve().parents[1]
        verifier_path = root / 'scripts/ccode/accept-offline-windows11-x64.ps1'
        self.assertTrue(verifier_path.is_file())
        verifier = verifier_path.read_text(encoding='utf-8')
        for requirement in (
            'assert-windows11-x64.ps1',
            'Get-NetRoute',
            "0.0.0.0/0",
            '::/0',
            'E_OFFLINE_ROUTE',
            'Get-Service',
            'Win32_SystemDriver',
            'E_OFFLINE_SERVICE',
            'E_OFFLINE_DRIVER',
            'programBefore',
            'programAfter',
            'dataRootManifest',
            'tools-integration.py',
            'loopbackFixture',
            'not a live model or enterprise gateway',
            'ConvertTo-SafeCommandEvidence',
            'interfaceAliasSha256',
            'nameSha256',
            'ConvertTo-Json',
        ):
            self.assertIn(requirement, verifier)

        # A connected GitHub runner cannot prove that a machine was physically
        # disconnected from all external networks. This verifier is intentionally
        # copied to, and run on, the standalone acceptance endpoint instead.
        tools_fixture = (root / 'tests/ccode/tools-integration.py').read_text(encoding='utf-8')
        self.assertIn('--acceptance-root', tools_fixture)
        self.assertIn('root_override', tools_fixture)

        for workflow_name in ('test-ccode.yml', 'test-ccode-windows11-x64.yml'):
            workflow = (root / '.github/workflows' / workflow_name).read_text(encoding='utf-8')
            self.assertNotIn('accept-offline-windows11-x64.ps1', workflow)

    def test_enterprise_acceptance_requires_signature_before_copy_or_execution(self):
        root = Path(__file__).resolve().parents[1]
        verifier = (root / 'scripts/ccode/accept-enterprise-lifecycle-windows11-x64.ps1').read_text(encoding='utf-8')
        for parameter in ('SignaturePath', 'PublicKeyPath', 'TrustedPin'):
            self.assertIn('[Parameter(Mandatory = $true)]\n    [string]$' + parameter, verifier)
        self.assertIn("'inspect-signed'", verifier)
        self.assertIn("$inspection.signatureVerification -ne 'passed'", verifier)
        self.assertLess(verifier.index("'inspect-signed'"), verifier.index('Copy-Item'))
        self.assertLess(verifier.index('E_LIFECYCLE_COPY'), verifier.index('& $platformGate'))
        self.assertIn('signedManifestSha256 = $inspection.signedManifestSha256', verifier)

    def test_enterprise_lifecycle_acceptance_uses_complete_candidate_and_external_data(self):
        root = Path(__file__).resolve().parents[1]
        verifier_path = root / 'scripts/ccode/accept-enterprise-lifecycle-windows11-x64.ps1'
        self.assertTrue(verifier_path.is_file())
        verifier = verifier_path.read_text(encoding='utf-8')
        for requirement in (
            '[string]$CandidateRoot',
            '[string]$EvidencePath',
            'enterprise_lifecycle.py',
            "'inspect-signed'",
            'package-audit.json',
            'assert-windows11-x64.ps1',
            'portable app 中文',
            'workspace 中文 with spaces',
            'persistent data',
            '--package-manifest',
            '--boundary-manifest',
            'tools-integration.py',
            'lifecycle-resume.py',
            'Move-Item',
            'workspaceIdentityBefore',
            'workspaceIdentityAfter',
            'E_LIFECYCLE_IDENTITY',
            'build_enterprise_package.py',
            "'compare'",
            'excludedDynamicData',
            'not a live model or enterprise gateway',
            'ConvertTo-Json',
        ):
            self.assertIn(requirement, verifier)
        self.assertNotIn('accept-enterprise-lifecycle-windows11-x64.ps1',
                         (root / '.github/workflows/test-ccode.yml').read_text(encoding='utf-8'))

        resume = (root / 'tests/ccode/lifecycle-resume.py')
        self.assertTrue(resume.is_file())
        resume_text = resume.read_text(encoding='utf-8')
        self.assertIn('--continue', resume_text)
        self.assertIn('original_prompt_marker', resume_text)
        self.assertIn('not a live model or enterprise gateway', resume_text)

    def test_failure_diagnostics_are_built_and_exercised_without_sensitive_values(self):
        root = Path(__file__).resolve().parents[1]
        build = (root / 'scripts/ccode/build.ps1').read_text(encoding='utf-8')
        self.assertIn('"diagnostic"', build)
        windows_test = (root / 'scripts/ccode/test-windows.ps1').read_text(encoding='utf-8')
        for requirement in (
            '--diagnostics',
            'diagnostic-super-secret',
            'sensitive diagnostic prompt',
            'E_GATEWAY',
            'operationId',
            'promptOrContentCaptured',
            'credentialsCaptured',
            'E_DIAGNOSTIC_WRITE',
            'diagnostic-sentinel',
        ):
            self.assertIn(requirement, windows_test)

    def test_failed_gateway_does_not_hide_independent_acceptance(self):
        workflow = (Path(__file__).resolve().parents[1] /
                    '.github/workflows/test-ccode.yml').read_text(encoding='utf-8')
        upload = workflow.split('      - uses: actions/upload-artifact@v4', 1)[1].split('\n  workspace-boundary:', 1)[0]
        self.assertIn("if: ${{ !cancelled() && steps.build.outcome == 'success' }}", upload)
        self.assertIn('name: ccode-windows-built-${{ matrix.version }}', upload)
        self.assertIn('if-no-files-found: error', upload)
        for name in ('workspace-boundary', 'cross-version'):
            job = workflow.split(f'\n  {name}:\n', 1)[1]
            header = job.split('    steps:', 1)[0]
            self.assertIn('needs: test', header)
            self.assertIn('if: ${{ !cancelled() }}', header)
        self.assertNotIn('ccode-windows-tested-', workflow)
        self.assertNotIn('continue-on-error:', workflow)


if __name__ == '__main__':
    unittest.main()
