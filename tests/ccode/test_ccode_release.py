from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class CcodeReleaseTests(unittest.TestCase):
    def test_ccode_is_appended_by_an_independent_follow_up_workflow(self):
        workflow = (ROOT / ".github/workflows/append-ccode-release.yml").read_text(
            encoding="utf-8"
        )

        self.assertIn("workflow_run:", workflow)
        self.assertIn("Auto Release AI Tools Portable", workflow)
        self.assertIn("workflow_dispatch:", workflow)
        self.assertIn("gh release upload", workflow)
        self.assertIn("ccode.exe", workflow)
        self.assertNotIn("gh release create", workflow)
        self.assertNotIn("gh release delete", workflow)

    def test_native_tests_cover_public_isolation_rules(self):
        native_test = (ROOT / "tests/ccode/native-tests.cpp").read_text(encoding="utf-8")

        for behavior in (
            "A_DEFAULT_HAIKU_MODEL",
            "C_DISABLE_NONESSENTIAL_TRAFFIC",
            r"data\\cc\\settings.json",
            "https://gateway.example.test",
            "https://api.deepseek.com/anthropic",
            "http://gateway.example.test",
        ):
            self.assertIn(behavior, native_test)

        self.assertIn('assert(!IsValidGatewayUrl(L""))', native_test)

    def test_build_produces_one_resource_packed_executable(self):
        build = (ROOT / "scripts/ccode/build.ps1").read_text(encoding="utf-8")

        self.assertIn("manifest.json", build)
        self.assertIn("Get-FileHash", build)
        self.assertIn("aa-runtime.bin", build)
        self.assertIn("cc-runtime.dll", build)
        self.assertIn("RCDATA", build)
        self.assertIn("ccode.exe", build)


if __name__ == "__main__":
    unittest.main()
