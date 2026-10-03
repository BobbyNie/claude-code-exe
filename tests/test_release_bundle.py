from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class ReleaseBundleTests(unittest.TestCase):
    def test_release_verification_waits_for_publication(self):
        workflow = read(".github/workflows/check-releases.yml")
        self.assertIn("  workflow_run:", workflow)
        self.assertIn("Auto Release AI Tools Portable", workflow)
        self.assertIn("github.event.workflow_run.conclusion == 'success'", workflow)
        self.assertNotIn("  push:", workflow)
        self.assertIn("cron: '0 1 * * *'", workflow)

    def test_main_changes_keep_native_regression_coverage(self):
        workflow = read(".github/workflows/test-ccode.yml")
        push = workflow.split("  push:", 1)[1].split("  pull_request:", 1)[0]
        self.assertIn("'main'", push)
        self.assertIn("'tests/**'", push)
        self.assertIn("'scripts/ccode/**'", push)

    def test_auto_release_workflow_publishes_one_bundle_release(self):
        workflow = read(".github/workflows/auto-release.yml")

        self.assertIn("Auto Release AI Tools Portable", workflow)
        self.assertIn(".\\scripts\\check-all-versions.ps1", workflow)

        for output_name in (
            "release_tag",
            "claude_version",
            "qwen_version",
            "codex_app_version",
            "codex_cli_version",
        ):
            self.assertIn(output_name, workflow)

        for asset in (
            "release\\claude.exe",
            "release\\qwen.exe",
            "release\\Codex.msix",
            "release\\codex.exe",
            "release\\README.txt",
        ):
            self.assertIn(asset, workflow)

        for notes_line in (
            "Claude Code:",
            "Qwen Code:",
            "Codex App:",
            "Codex CLI:",
        ):
            self.assertIn(notes_line, workflow)


    def test_qwen_release_workflow_is_not_separate_anymore(self):
        self.assertFalse((ROOT / ".github/workflows/auto-release-qwencode.yml").exists())


    def test_version_check_models_bundle_completeness(self):
        script = read("scripts/check-all-versions.ps1")

        for function_name in (
            "Get-ClaudeLatestVersion",
            "Get-QwenLatestVersion",
            "Get-CodexAppPackageInfo",
            "Get-CodexAppLatestVersion",
            "Get-CodexCliLatestVersion",
            "New-BundleReleaseTag",
            "Test-BundleReleaseComplete",
        ):
            self.assertIn(f"function {function_name}", script)

        for required_asset in (
            '"claude.exe"',
            '"qwen.exe"',
            '"Codex.msix"',
            '"codex.exe"',
            '"README.txt"',
        ):
            self.assertIn(required_asset, script)


    def test_codex_download_scripts_use_official_sources_and_stable_output_names(self):
        app_script = read("scripts/codex/download-app.ps1")
        cli_script = read("scripts/codex/download-cli.ps1")

        self.assertIn("GetStoreURL.ps1", app_script)
        self.assertIn("Get-StoreURLs", app_script)
        self.assertIn("fe3.delivery.mp.microsoft.com", app_script)
        self.assertNotIn("winget download", app_script)
        self.assertIn("9PLM9XGG6VKS", app_script)
        self.assertIn("Codex.msix", app_script)

        self.assertIn("openai/codex", cli_script)
        self.assertIn("codex-x86_64-pc-windows-msvc.exe", cli_script)
        self.assertIn("codex.exe", cli_script)


    def test_release_checker_verifies_the_unified_bundle(self):
        script = read("scripts/check-github-releases.ps1")

        self.assertIn("check-all-versions.ps1", script)
        self.assertIn("Test-BundleReleaseComplete", script)

        self.assertIn("Get-BundleRequiredAssets", script)



if __name__ == "__main__":
    unittest.main()
