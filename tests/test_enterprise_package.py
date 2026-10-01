"""Enterprise delivery assembly is allowlisted, deterministic, and fail closed."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[1]
BUILDER = ROOT / "scripts/ccode/build_enterprise_package.py"
USAGE = ROOT / "docs/ccode-enterprise-usage.md"


def sha256(contents):
    return hashlib.sha256(contents).hexdigest()


class EnterprisePackageTests(unittest.TestCase):
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
        }, sort_keys=True), encoding="utf-8")
        self.boundary_document = {
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
            "notices": {
                "source": "enterprise-package-manifest",
                "launcherRewrites": False,
            },
            "sideEffects": {
                "createsData": False,
                "createsProfile": False,
                "extractsRuntime": False,
            },
        }
        self.boundary = self.root / "runtime-boundary.json"
        self.boundary.write_text(json.dumps(self.boundary_document, sort_keys=True), encoding="utf-8")

    def tearDown(self):
        self.temporary.cleanup()

    def command(self, output, *, restricted="restricted", notice_hash=None,
                usage=USAGE, provenance=None, boundary=None, notice=None):
        return [
            sys.executable, str(BUILDER),
            "--executable", str(self.executable),
            "--provenance", str(provenance or self.provenance),
            "--boundary", str(boundary or self.boundary),
            "--usage", str(usage),
            "--notice", str(notice or self.notice),
            "--notice-sha256", notice_hash or sha256(self.notice_bytes),
            "--restricted-name", restricted,
            "--output", str(output),
        ]

    def run_builder(self, output, **kwargs):
        return subprocess.run(self.command(output, **kwargs), capture_output=True, text=True)

    def test_builds_only_allowlisted_windows11_x64_files_and_pairs_zip_with_unpacked(self):
        output = self.root / "delivery"
        completed = self.run_builder(output)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        result = json.loads(completed.stdout)
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["platform"], "windows")
        self.assertEqual(result["architecture"], "x64")
        self.assertEqual(result["minimumWindowsBuild"], 22000)
        self.assertEqual(result["audit"]["status"], "passed")
        self.assertEqual(result["audit"]["comparison"], "matched")

        archive = output / result["archive"]
        unpacked = output / result["unpacked"]
        audit_path = output / result["auditReport"]
        expected = {
            "ccode.exe",
            "docs/usage.md",
            "manifest.json",
            "notices/NOTICE.txt",
        }
        with zipfile.ZipFile(archive) as package:
            self.assertEqual(set(package.namelist()), expected)
            self.assertEqual(package.read("ccode.exe"), self.executable.read_bytes())
            self.assertEqual(package.read("notices/NOTICE.txt"), self.notice_bytes)
            manifest = json.loads(package.read("manifest.json"))
        self.assertEqual(
            {path.relative_to(unpacked).as_posix() for path in unpacked.rglob("*") if path.is_file()},
            expected,
        )
        self.assertEqual((unpacked / "notices/NOTICE.txt").read_bytes(), self.notice_bytes)
        self.assertEqual(json.loads(audit_path.read_text(encoding="utf-8"))["status"], "passed")

        self.assertEqual(manifest["schemaVersion"], 1)
        self.assertEqual(manifest["packageName"], "ccode-enterprise")
        self.assertEqual(manifest["platform"], "windows")
        self.assertEqual(manifest["architecture"], "x64")
        self.assertEqual(manifest["minimumWindowsBuild"], 22000)
        self.assertEqual(manifest["executable"]["sha256"], sha256(self.executable.read_bytes()))
        self.assertEqual(manifest["provenance"]["adapterRevision"], "a" * 40)
        self.assertEqual(manifest["provenance"]["engineVersion"], "2.1.282")
        self.assertEqual(manifest["runtimeBoundary"], self.boundary_document)
        self.assertEqual(manifest["notices"], [{
            "path": "notices/NOTICE.txt",
            "sha256": sha256(self.notice_bytes),
            "size": len(self.notice_bytes),
        }])
        self.assertEqual(manifest["publicBoundary"]["opaqueContents"], ["ccode.exe"])
        self.assertEqual(manifest["redistributionApproval"], "external-gate-not-asserted")
        self.assertEqual(
            {entry["path"] for entry in manifest["files"]},
            {"ccode.exe", "docs/usage.md", "notices/NOTICE.txt"},
        )
        for forbidden in ("data", "runtime", "profile", "session", "temp"):
            self.assertFalse(any(forbidden in member.casefold() for member in expected))

    def test_same_inputs_produce_identical_archive_bytes(self):
        first = self.root / "first"
        second = self.root / "second"
        first_result = self.run_builder(first)
        second_result = self.run_builder(second)
        self.assertEqual(first_result.returncode, 0, first_result.stderr)
        self.assertEqual(second_result.returncode, 0, second_result.stderr)
        first_name = json.loads(first_result.stdout)["archive"]
        second_name = json.loads(second_result.stdout)["archive"]
        self.assertEqual((first / first_name).read_bytes(), (second / second_name).read_bytes())

    def test_notice_is_mandatory_hash_bound_and_never_silently_rewritten(self):
        output = self.root / "bad-notice"
        mismatch = self.run_builder(output, notice_hash="0" * 64)
        self.assertEqual(mismatch.returncode, 2)
        self.assertEqual(json.loads(mismatch.stdout)["code"], "E_NOTICE_HASH")
        self.assertFalse(output.exists())

        missing = subprocess.run([
            sys.executable, str(BUILDER),
            "--executable", str(self.executable),
            "--provenance", str(self.provenance),
            "--usage", str(USAGE),
            "--restricted-name", "restricted",
            "--output", str(self.root / "missing-notice"),
        ], capture_output=True, text=True)
        self.assertNotEqual(missing.returncode, 0)
        self.assertFalse((self.root / "missing-notice").exists())

    def test_rejects_invalid_provenance_mixed_output_and_non_regular_inputs(self):
        invalid = self.root / "invalid-provenance.json"
        invalid.write_text(json.dumps({"platform": "windows", "architecture": "arm64"}), encoding="utf-8")
        failed = self.run_builder(self.root / "invalid-output", provenance=invalid)
        self.assertEqual(failed.returncode, 2)
        self.assertEqual(json.loads(failed.stdout)["code"], "E_PROVENANCE")

        mixed = self.root / "mixed"
        mixed.mkdir()
        (mixed / "other-tool.exe").write_bytes(b"MZother")
        failed = self.run_builder(mixed)
        self.assertEqual(failed.returncode, 2)
        self.assertEqual(json.loads(failed.stdout)["code"], "E_OUTPUT_EXISTS")
        self.assertEqual((mixed / "other-tool.exe").read_bytes(), b"MZother")

        link = self.root / "linked-notice.txt"
        try:
            link.symlink_to(self.notice)
        except OSError:
            self.skipTest("symlinks unavailable")
        failed = self.run_builder(self.root / "linked-output", notice=link)
        self.assertEqual(failed.returncode, 2)
        self.assertEqual(json.loads(failed.stdout)["code"], "E_INPUT_TYPE")

    def test_boundary_rejects_boolean_numeric_substitution_without_output(self):
        for label, change in (
            ("schema-bool", lambda doc: doc.update(schemaVersion=True)),
            ("schema-float", lambda doc: doc.update(schemaVersion=1.0)),
            ("runtime-number", lambda doc: doc["childRuntimeEnvironment"].update(originalRuntimeNamesPresent=1)),
            ("side-effect-number", lambda doc: doc["sideEffects"].update(createsData=0)),
        ):
            with self.subTest(label=label):
                document = json.loads(json.dumps(self.boundary_document))
                change(document)
                invalid = self.root / (label + ".json")
                invalid.write_text(json.dumps(document), encoding="utf-8")
                output = self.root / (label + "-output")
                result = self.run_builder(output, boundary=invalid)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("E_BOUNDARY", result.stdout + result.stderr)
                self.assertFalse(output.exists())

    def test_boundary_manifest_is_required_and_must_match_windows11_x64_contract(self):
        missing = subprocess.run([
            sys.executable, str(BUILDER),
            "--executable", str(self.executable),
            "--provenance", str(self.provenance),
            "--usage", str(USAGE),
            "--notice", str(self.notice),
            "--notice-sha256", sha256(self.notice_bytes),
            "--restricted-name", "restricted",
            "--output", str(self.root / "missing-boundary"),
        ], capture_output=True, text=True)
        self.assertNotEqual(missing.returncode, 0)
        self.assertFalse((self.root / "missing-boundary").exists())

        for field, value in (
            ("schemaVersion", 2),
            ("platform", "linux"),
            ("architecture", "arm64"),
            ("minimumWindowsBuild", 21999),
        ):
            invalid_document = dict(self.boundary_document)
            invalid_document[field] = value
            invalid = self.root / f"invalid-boundary-{field}.json"
            invalid.write_text(json.dumps(invalid_document), encoding="utf-8")
            output = self.root / f"invalid-boundary-{field}"
            failed = self.run_builder(output, boundary=invalid)
            self.assertEqual(failed.returncode, 2)
            self.assertEqual(json.loads(failed.stdout)["code"], "E_BOUNDARY")
            self.assertFalse(output.exists())

    def test_name_or_required_notice_conflict_fails_closed_without_delivery(self):
        conflicting_usage = self.root / "usage.md"
        conflicting_usage.write_text("Contains Restricted public wording", encoding="utf-8")
        output = self.root / "conflict"
        failed = self.run_builder(output, usage=conflicting_usage)
        self.assertEqual(failed.returncode, 2)
        report = json.loads(failed.stdout)
        self.assertEqual(report["code"], "E_PACKAGE_AUDIT")
        self.assertEqual(report["audit"]["status"], "failed")
        self.assertFalse(output.exists())

        conflicting_notice = self.root / "NOTICE-conflict.txt"
        conflict_bytes = b"Required Restricted attribution must remain exact"
        conflicting_notice.write_bytes(conflict_bytes)
        output = self.root / "notice-conflict"
        failed = self.run_builder(
            output,
            notice=conflicting_notice,
            notice_hash=sha256(conflict_bytes),
        )
        self.assertEqual(failed.returncode, 2)
        report = json.loads(failed.stdout)
        self.assertEqual(report["code"], "E_PACKAGE_AUDIT")
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
