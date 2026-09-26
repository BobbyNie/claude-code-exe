"""Enterprise lifecycle candidates are revalidated before and after Windows trials."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILDER = ROOT / "scripts/ccode/build_enterprise_package.py"
LIFECYCLE = ROOT / "scripts/ccode/enterprise_lifecycle.py"
USAGE = ROOT / "docs/ccode-enterprise-usage.md"


def sha256(contents):
    return hashlib.sha256(contents).hexdigest()


class EnterpriseLifecycleTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.executable = self.root / "candidate.exe"
        self.executable.write_bytes(b"MZ\x00ccode-wrapper")
        self.notice = self.root / "NOTICE.txt"
        self.notice_bytes = b"Required neutral redistribution notice\r\n"
        self.notice.write_bytes(self.notice_bytes)
        self.provenance = self.root / "package-provenance.json"
        self.provenance.write_text(json.dumps({
            "schemaVersion": 1,
            "packageName": "ccode",
            "packageVersion": "1.0",
            "platform": "windows",
            "architecture": "x64",
            "adapterRevision": "a" * 40,
            "engineVersion": "2.1.282",
            "engineSha256": "b" * 64,
            "engineSize": 123456,
            "officialManifestUrl": "https://downloads.example.test/2.1.282/manifest.json",
            "officialManifestSha256": "c" * 64,
            "officialPayloadUrl": "https://downloads.example.test/2.1.282/win32-x64/engine.exe",
        }), encoding="utf-8")
        self.boundary = self.root / "runtime-boundary.json"
        self.boundary.write_text(json.dumps({
            "schemaVersion": 1,
            "platform": "windows",
            "architecture": "x64",
            "minimumWindowsBuild": 22000,
            "publicEnvironment": {
                "acceptedExact": ["A_API_KEY", "A_AUTH_TOKEN", "A_BASE_URL", "CCODE_DATA_DIR"],
                "acceptedPrefixes": ["A_", "C_"],
                "valuesRecorded": False,
            },
            "childRuntimeEnvironment": {
                "inheritedFiltering": {
                    "removedPrefixes": ["ANTHROPIC_", "CLAUDE_"],
                    "removedExact": ["CLAUDECODE"],
                },
                "aliasExpansion": [
                    {"publicPrefix": "A_", "runtimePrefix": "ANTHROPIC_"},
                    {"publicPrefix": "C_", "runtimePrefix": "CLAUDE_CODE_"},
                ],
                "profileRelative": {
                    "APPDATA": "roaming", "HOME": "home", "LOCALAPPDATA": "local",
                    "TEMP": "temp", "TMP": "temp", "USERPROFILE": "home",
                },
                "fixedValues": {
                    "CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC": "1",
                    "CLAUDE_CODE_DISABLE_NONSTREAMING_FALLBACK": "1",
                    "CLAUDE_CODE_MAX_RETRIES": "0",
                    "CLAUDE_CODE_RETRY_WATCHDOG": "0",
                    "DISABLE_AUTOUPDATER": "1",
                },
                "originalRuntimeNamesPresent": True,
                "processTreeNameFree": False,
                "valuesRecorded": False,
            },
            "binaryMetadata": {
                "peResources": [
                    {"id": 101, "purpose": "opaque-embedded-engine", "contentsScannedForNames": False},
                    {"id": 102, "purpose": "validated-package-provenance-json", "contentsScannedForNames": False},
                ],
                "publisherSignature": "not-asserted",
                "opaqueBinaryNameScan": "not-performed",
            },
            "notices": {"source": "enterprise-package-manifest", "launcherRewrites": False},
            "sideEffects": {"createsData": False, "createsProfile": False, "extractsRuntime": False},
        }), encoding="utf-8")

    def tearDown(self):
        self.temporary.cleanup()

    def build(self, output):
        completed = subprocess.run([
            sys.executable, str(BUILDER),
            "--executable", str(self.executable),
            "--provenance", str(self.provenance),
            "--boundary", str(self.boundary),
            "--usage", str(USAGE),
            "--notice", str(self.notice),
            "--notice-sha256", sha256(self.notice_bytes),
            "--restricted-name", "restricted",
            "--output", str(output),
        ], capture_output=True, text=True)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)

    def run_lifecycle(self, *arguments):
        return subprocess.run([sys.executable, str(LIFECYCLE), *map(str, arguments)],
                              capture_output=True, text=True)

    def test_inspect_recomputes_candidate_and_returns_repack_inputs(self):
        candidate = self.root / "candidate"
        self.build(candidate)
        completed = self.run_lifecycle("inspect", "--candidate-root", candidate)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        report = json.loads(completed.stdout)
        self.assertEqual(report["status"], "passed")
        self.assertEqual(report["platform"], "windows")
        self.assertEqual(report["architecture"], "x64")
        self.assertEqual(report["minimumWindowsBuild"], 22000)
        self.assertEqual(report["restrictedNames"], ["restricted"])
        self.assertEqual(report["unpackedFiles"], json.loads(
            (candidate / "package-audit.json").read_text())["unpacked"]["files"])
        self.assertEqual(report["manifest"]["provenance"]["engineVersion"], "2.1.282")
        self.assertEqual(report["usagePath"], "docs/usage.md")
        self.assertEqual(report["noticePaths"], ["notices/NOTICE.txt"])

    def test_inspect_rejects_tampering_even_when_saved_audit_still_says_passed(self):
        candidate = self.root / "tampered"
        self.build(candidate)
        (candidate / "unpacked/docs/usage.md").write_text("tampered", encoding="utf-8")
        completed = self.run_lifecycle("inspect", "--candidate-root", candidate)
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(json.loads(completed.stdout)["code"], "E_LIFECYCLE_AUDIT")

    def test_compare_requires_identical_unpacked_delivery_but_records_archive_digests(self):
        original = self.root / "original"
        repacked = self.root / "repacked"
        self.build(original)
        self.build(repacked)
        completed = self.run_lifecycle("compare", "--original", original, "--repacked", repacked)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        report = json.loads(completed.stdout)
        self.assertEqual(report["comparison"], "matched")
        self.assertTrue(report["archiveBytesIdentical"])

        (repacked / "unpacked/docs/usage.md").write_text("changed", encoding="utf-8")
        mismatch = self.run_lifecycle("compare", "--original", original, "--repacked", repacked)
        self.assertEqual(mismatch.returncode, 2)
        self.assertEqual(json.loads(mismatch.stdout)["code"], "E_LIFECYCLE_AUDIT")


if __name__ == "__main__":
    unittest.main()
