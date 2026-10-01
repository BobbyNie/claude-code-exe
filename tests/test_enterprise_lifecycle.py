"""Enterprise lifecycle candidates are revalidated before and after Windows trials."""
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

    def test_signed_inspection_requires_real_signature_and_independent_pin(self):
        candidate = self.root / "signed-candidate"
        self.build(candidate)
        signature = self.root / "manifest.sig"
        key = self.root / "signer.der"
        signer = subprocess.run(["node", "--input-type=module", "-e", """
            import {generateKeyPairSync, sign, createHash} from 'node:crypto';
            import {readFileSync, writeFileSync} from 'node:fs';
            const [manifestPath, signaturePath, keyPath] = process.argv.slice(1);
            const {publicKey, privateKey} = generateKeyPairSync('ed25519');
            const der = publicKey.export({type:'spki', format:'der'});
            writeFileSync(keyPath, der);
            writeFileSync(signaturePath, sign(null, Buffer.concat([
              Buffer.from('ccode-enterprise-manifest-v1\\0'), readFileSync(manifestPath)
            ]), privateKey));
            console.log(createHash('sha256').update(der).digest('hex'));
        """, str(candidate / "unpacked/manifest.json"), str(signature), str(key)],
            capture_output=True, text=True)
        self.assertEqual(signer.returncode, 0, signer.stderr)
        pin = signer.stdout.strip()
        arguments = ("inspect-signed", "--candidate-root", candidate,
                     "--signature", signature, "--public-key", key, "--trusted-pin")
        completed = self.run_lifecycle(*arguments, pin)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertEqual(json.loads(completed.stdout)["signatureVerification"], "passed")
        rejected = self.run_lifecycle(*arguments, "0" * 64)
        self.assertEqual(rejected.returncode, 2)
        self.assertEqual(json.loads(rejected.stdout)["code"], "E_LIFECYCLE_SIGNATURE")
        manifest_path = candidate / "unpacked/manifest.json"
        changed = json.loads(manifest_path.read_text())
        changed["provenance"]["packageVersion"] = "unsigned-change"
        manifest_path.write_text(json.dumps(changed), encoding="utf-8")
        self.refresh_candidate_audit(candidate)
        unsigned = self.run_lifecycle("inspect", "--candidate-root", candidate)
        self.assertEqual(unsigned.returncode, 0, unsigned.stdout + unsigned.stderr)
        stale_signature = self.run_lifecycle(*arguments, pin)
        self.assertEqual(stale_signature.returncode, 2)
        self.assertEqual(json.loads(stale_signature.stdout)["code"], "E_LIFECYCLE_SIGNATURE")

    @unittest.skipUnless(sys.platform == "win32", "Requires Windows PowerShell execution")
    def test_windows_acceptance_rejects_unsigned_candidate_before_working_files(self):
        candidate = self.root / "unsigned-windows-candidate"
        self.build(candidate)
        signature = self.root / "invalid.sig"
        key = self.root / "invalid.der"
        signature.write_bytes(bytes(64))
        key.write_bytes(b"invalid-public-key")
        working = self.root / "must-not-be-created"
        evidence = self.root / "must-not-be-written.json"
        completed = subprocess.run(["pwsh", "-NoProfile", "-NonInteractive", "-File",
            str(ROOT / "scripts/ccode/accept-enterprise-lifecycle-windows11-x64.ps1"),
            "-CandidateRoot", str(candidate), "-EvidencePath", str(evidence),
            "-SignaturePath", str(signature), "-PublicKeyPath", str(key),
            "-TrustedPin", "0" * 64, "-WorkingRoot", str(working)],
            capture_output=True, text=True, timeout=60)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("E_LIFECYCLE_COMMAND", completed.stdout + completed.stderr)
        self.assertFalse(working.exists())
        self.assertFalse(evidence.exists())

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

    def test_inspect_rejects_numeric_boundary_even_with_recomputed_matching_audit(self):
        candidate = self.root / "numeric-boundary"
        self.build(candidate)
        unpacked = candidate / "unpacked"
        manifest_path = unpacked / "manifest.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["runtimeBoundary"]["childRuntimeEnvironment"]["originalRuntimeNamesPresent"] = 1
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        self.refresh_candidate_audit(candidate)
        completed = self.run_lifecycle("inspect", "--candidate-root", candidate)
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(json.loads(completed.stdout)["code"], "E_LIFECYCLE_MANIFEST")

    def refresh_candidate_audit(self, candidate):
        unpacked = candidate / "unpacked"
        archive = next(candidate.glob("*.zip"))
        with zipfile.ZipFile(archive, "w") as bundle:
            for source in sorted(unpacked.rglob("*")):
                if source.is_file():
                    bundle.write(source, source.relative_to(unpacked).as_posix())
        audit = subprocess.run([sys.executable, str(ROOT / "scripts/ccode/package_audit.py"),
            str(archive), "--archive", "--unpacked", str(unpacked),
            "--restricted-name", "restricted", "--opaque", "ccode.exe"],
            capture_output=True, text=True)
        self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)
        (candidate / "package-audit.json").write_text(audit.stdout, encoding="utf-8")

    def test_inspect_rejects_non_https_provenance_with_matching_audit(self):
        candidate = self.root / "unsafe-provenance"
        self.build(candidate)
        manifest_path = candidate / "unpacked/manifest.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["provenance"]["officialPayloadUrl"] = "http://downloads.example.test/engine.exe"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        self.refresh_candidate_audit(candidate)
        completed = self.run_lifecycle("inspect", "--candidate-root", candidate)
        self.assertEqual(completed.returncode, 2)
        self.assertEqual(json.loads(completed.stdout)["code"], "E_LIFECYCLE_MANIFEST")

    def test_inspect_rejects_duplicate_saved_audit_keys(self):
        candidate = self.root / "duplicate-audit"
        self.build(candidate)
        audit = candidate / "package-audit.json"
        raw = audit.read_text(encoding="utf-8")
        audit.write_text('{"status":"failed",' + raw.lstrip()[1:], encoding="utf-8")
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
