"""Supplementary wiring checks; Win32 behavior is exercised by the build script."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class EnterpriseStartupWiringTests(unittest.TestCase):
    def test_enterprise_gate_precedes_every_entry_point(self):
        source = (ROOT / 'scripts/ccode/launcher.cpp').read_text()
        main = source[source.index('int Main('):source.index('int wmain(')]
        gate = main.index('NativeEnterpriseGate<CompiledSignerPolicy> enterpriseGate')
        self.assertLess(gate, main.index('return PermissionServer()'))
        self.assertLess(gate, main.index('L"--help"'))
        self.assertLess(gate, main.index('auto metadata = Metadata()'))
        self.assertLess(gate, main.index('fs::create_directories(options.data)'))
        self.assertIn('#ifdef CCODE_ENTERPRISE_REQUIRED', source)

    def test_build_exposes_paired_explicit_policy_and_runs_native_entry_tests(self):
        source = (ROOT / 'scripts/ccode/build.ps1').read_text()
        self.assertIn('$SignerSpkiPath', source)
        self.assertIn('$ApprovedSignerPin', source)
        self.assertIn('generate_enterprise_policy.py', source)
        self.assertIn('/DCCODE_ENTERPRISE_REQUIRED', source)
        self.assertIn('test-enterprise-startup.ps1', source)

    def test_build_runs_signed_enterprise_launcher_fixture(self):
        source = (ROOT / 'scripts/ccode/build.ps1').read_text()
        self.assertIn('Invoke-Checked $signatureTest $fixtureExe $metadataPath --enterprise-launcher', source)

    def test_enterprise_data_gate_precedes_persistent_side_effects(self):
        source = (ROOT / 'scripts/ccode/launcher.cpp').read_text()
        self.assertLess(source.index('LockedEnterpriseData enterpriseData'),
                        source.index('fs::create_directories(options.data)'))

    def test_lifecycle_installs_signature_before_first_candidate_execution(self):
        source = (ROOT / 'scripts/ccode/accept-enterprise-lifecycle-windows11-x64.ps1').read_text()
        installation = source.index("$lifecycleTool, 'install-signature'")
        self.assertLess(installation, source.index('& $platformGate -Executable $app'))
        self.assertIn('$signatureInstallation.installedSignatureSha256', source)

    def test_runtime_owner_reaches_all_actual_engine_calls(self):
        source = (ROOT / 'scripts/ccode/launcher.cpp').read_text()
        self.assertIn('ccode::RetainedRuntimePayload PrepareRuntime(', source)
        self.assertIn('return ccode::RetainedRuntimePayload(payload, resource.size, hash)', source)
        self.assertNotIn('RunTurn(module, payload,', source)
        self.assertEqual(source.count('RunTurn(module, payload.Path(),'), 4)
