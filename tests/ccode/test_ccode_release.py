from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class CcodeReleaseTests(unittest.TestCase):
    def test_ccode_is_never_published_to_the_mixed_public_bundle(self):
        legacy = ROOT / ".github/workflows/append-ccode-release.yml"
        self.assertFalse(legacy.exists())

        for workflow_path in (ROOT / ".github/workflows").glob("*.yml"):
            workflow = workflow_path.read_text(encoding="utf-8")
            publishes_ccode = "gh release upload" in workflow and "ccode.exe" in workflow
            targets_mixed_bundle = "Auto Release AI Tools Portable" in workflow
            self.assertFalse(
                publishes_ccode and targets_mixed_bundle,
                f"{workflow_path.name} still publishes ccode into the mixed bundle",
            )

    def test_native_tests_cover_public_isolation_rules(self):
        native_test = (ROOT / "tests/ccode/native-tests.cpp").read_text(encoding="utf-8")

        for behavior in (
            "A_DEFAULT_HAIKU_MODEL",
            "A_AUTH_TOKEN",
            "C_DISABLE_NONESSENTIAL_TRAFFIC",
            "https://gateway.example.test",
            "https://api.deepseek.com/anthropic",
            "http://gateway.example.test",
        ):
            self.assertIn(behavior, native_test)

        self.assertIn('assert(!IsValidGatewayUrl(L""))', native_test)

    def test_build_embeds_verified_package_provenance(self):
        build = (ROOT / "scripts/ccode/build.ps1").read_text(encoding="utf-8")

        for contract in (
            "[string]$AdapterRevision",
            "officialManifestUrl",
            "officialManifestSha256",
            "officialPayloadUrl",
            "adapterRevision",
            "engineSize",
            "schemaVersion",
        ):
            self.assertIn(contract, build)

        self.assertIn("Get-FileHash -Path $manifestPath -Algorithm SHA256", build)
        self.assertNotIn("[string]$AdapterRevision =", build)

    def test_launcher_exposes_verified_package_manifest(self):
        launcher = (ROOT / "scripts/ccode/launcher.cpp").read_text(encoding="utf-8")

        self.assertIn('L"--package-manifest"', launcher)
        self.assertIn('"officialManifestSha256"', launcher)
        self.assertIn('Digest(payload)', launcher)

    def test_launcher_exposes_side_effect_free_runtime_boundary_manifest(self):
        launcher = (ROOT / "scripts/ccode/launcher.cpp").read_text(encoding="utf-8")

        self.assertIn('#include "boundary.hpp"', launcher)
        self.assertIn('L"--boundary-manifest"', launcher)
        self.assertIn('ccode::BoundaryManifest().dump()', launcher)
        self.assertIn('--boundary-manifest   Public/runtime/binary boundary as JSON', launcher)
        handler = launcher.index('if (argc == 2 && std::wstring(argv[1]) == L"--boundary-manifest")')
        self.assertLess(handler, launcher.index("auto options = Parse(argc, argv, module);"))
        self.assertLess(handler, launcher.index("fs::create_directories(options.data);"))

    def test_windows11_acceptance_records_package_provenance(self):
        workflow = (ROOT / ".github/workflows/test-ccode-windows11-x64.yml").read_text(
            encoding="utf-8"
        )

        self.assertIn("-AdapterRevision '${{ github.sha }}'", workflow)
        self.assertIn("package-manifest-integration.py", workflow)
        self.assertIn("package-provenance.json", workflow)

    def test_build_produces_one_resource_packed_executable(self):
        build = (ROOT / "scripts/ccode/build.ps1").read_text(encoding="utf-8")

        self.assertIn("manifest.json", build)
        self.assertIn("Get-FileHash", build)
        self.assertIn("aa-runtime.exe", build)
        self.assertNotIn("cc-runtime.dll", build)
        self.assertIn("RCDATA", build)
        self.assertIn("ccode.exe", build)
        self.assertNotIn("MinHook", build)
        self.assertIn("package.json", build)


if __name__ == "__main__":
    unittest.main()
