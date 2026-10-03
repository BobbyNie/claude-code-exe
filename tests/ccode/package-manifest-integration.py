"""Windows 11 x64 acceptance: verify and record embedded package provenance."""
import argparse
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import platform
from pathlib import Path
import subprocess
import sys
import tempfile


def load_resources(executable):
    # Inspect the PE image as data. Do not execute an entry point while extracting evidence.
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    pointer = ctypes.c_void_p
    kernel.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, pointer, ctypes.c_uint32]
    kernel.LoadLibraryExW.restype = pointer
    kernel.FindResourceW.argtypes = [pointer, pointer, pointer]
    kernel.FindResourceW.restype = pointer
    kernel.SizeofResource.argtypes = [pointer, pointer]
    kernel.SizeofResource.restype = ctypes.c_uint32
    kernel.LoadResource.argtypes = [pointer, pointer]
    kernel.LoadResource.restype = pointer
    kernel.LockResource.argtypes = [pointer]
    kernel.LockResource.restype = pointer
    kernel.FreeLibrary.argtypes = [pointer]
    kernel.FreeLibrary.restype = ctypes.c_int

    module = kernel.LoadLibraryExW(str(executable), None, 2)  # LOAD_LIBRARY_AS_DATAFILE
    if not module:
        raise RuntimeError("Unable to load package resources")
    try:
        result = []
        for identifier in (101, 102):
            resource = kernel.FindResourceW(module, identifier, 10)  # RT_RCDATA
            size = kernel.SizeofResource(module, resource) if resource else 0
            loaded = kernel.LoadResource(module, resource) if resource else None
            address = kernel.LockResource(loaded) if loaded else None
            if not size or not address:
                raise RuntimeError(f"Missing or unreadable package resource {identifier}")
            result.append(ctypes.string_at(address, size))
        return result
    finally:
        kernel.FreeLibrary(module)


def sha256(contents):
    return hashlib.sha256(contents).hexdigest()


def environment_record(version, machine, bits, administrator, target):
    if machine.upper() not in ('AMD64', 'X86_64') or bits != 64:
        raise ValueError('Acceptance requires Windows x64 and a 64-bit verifier')
    if target not in ('github-hosted', 'windows11-ordinary'):
        raise ValueError('Unknown acceptance target')
    if target == 'windows11-ordinary' and (
            version.major != 10 or version.build < 22000 or version.product_type != 1 or administrator):
        raise ValueError('Windows 11 ordinary-account evidence requires that actual environment')
    return {'acceptanceTarget': ('GitHub hosted Windows x64' if target == 'github-hosted'
                                else 'Windows 11 x64 ordinary account'),
        'osMajor': version.major, 'osMinor': version.minor, 'osBuild': version.build,
        'windowsProductType': version.product_type, 'architecture': 'x64',
        'processBits': bits, 'administratorToken': administrator}


def check(executable, expected_version, adapter_revision, evidence_path, target="windows11-ordinary"):
    if sys.platform != "win32":
        raise RuntimeError("Windows package acceptance requires Windows")
    if not (len(adapter_revision) in (40, 64) and all(ch in "0123456789abcdefABCDEF" for ch in adapter_revision)):
        raise ValueError("Adapter revision must be a full hexadecimal commit ID")

    shell = ctypes.WinDLL("shell32", use_last_error=True)
    shell.IsUserAnAdmin.argtypes = []
    shell.IsUserAnAdmin.restype = ctypes.c_int
    environment = environment_record(sys.getwindowsversion(), platform.machine(),
        ctypes.sizeof(ctypes.c_void_p) * 8, bool(shell.IsUserAnAdmin()), target)
    executable = Path(executable).resolve(strict=True)
    adapter_revision = adapter_revision.lower()
    payload, metadata_bytes = load_resources(executable)
    embedded_manifest = json.loads(metadata_bytes.decode("utf-8-sig"))
    base = (
        "https://storage.googleapis.com/"
        "claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/"
        f"claude-code-releases/{expected_version}"
    )
    expected_manifest = {
        "schemaVersion": 1,
        "packageName": "ccode",
        "packageVersion": "1.0",
        "platform": "windows",
        "architecture": "x64",
        "adapterRevision": adapter_revision,
        "engineVersion": expected_version,
        "engineSha256": sha256(payload),
        "engineSize": len(payload),
        "officialManifestUrl": f"{base}/manifest.json",
        "officialManifestSha256": embedded_manifest.get("officialManifestSha256"),
        "officialPayloadUrl": f"{base}/win32-x64/claude.exe",
    }
    assert embedded_manifest == expected_manifest, "Embedded package manifest does not match the build inputs"
    manifest_hash = embedded_manifest["officialManifestSha256"]
    assert len(manifest_hash) == 64 and all(ch in "0123456789abcdef" for ch in manifest_hash), (
        "Official manifest digest is not lowercase SHA256"
    )

    with tempfile.TemporaryDirectory(prefix="ccode-provenance-") as temporary:
        root = Path(temporary)
        public = subprocess.run(
            [str(executable), "--package-manifest"], cwd=root, capture_output=True, timeout=30
        )
        assert public.returncode == 0, "Public package manifest command failed"
        assert public.stderr == b"", "Public package manifest emitted an error"
        assert json.loads(public.stdout.decode("utf-8")) == expected_manifest, (
            "Public package manifest differs from the embedded manifest"
        )
        assert list(root.iterdir()) == [], "Package manifest command created profile/runtime side effects"

        extracted = root / "extracted-engine.exe"
        extracted.write_bytes(payload)
        extracted_hash = sha256(extracted.read_bytes())
        extracted_size = extracted.stat().st_size
        assert extracted_hash == expected_manifest["engineSha256"]
        assert extracted_size == expected_manifest["engineSize"]

        self_test = subprocess.run(
            [str(executable), "--ccode-self-test"], cwd=root, capture_output=True, timeout=30
        )
        assert self_test.returncode == 0 and self_test.stdout.strip() == b"ccode self-test ok"
        assert self_test.stderr == b""
        assert sorted(path.name for path in root.iterdir()) == ["extracted-engine.exe"], (
            "Self-test created profile/runtime side effects"
        )

    evidence = {
        "schemaVersion": 1,
        "recordedAtUtc": datetime.now(timezone.utc).isoformat(),
        "acceptanceTarget": environment["acceptanceTarget"],
        "environment": environment,
        "packageExecutableSha256": sha256(executable.read_bytes()),
        "embeddedManifestSha256": sha256(metadata_bytes),
        "extractedEngineSha256": extracted_hash,
        "extractedEngineSize": extracted_size,
        "manifest": expected_manifest,
    }
    evidence_path = Path(evidence_path)
    evidence_path.parent.mkdir(parents=True, exist_ok=True)
    evidence_path.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("PASS: verified public provenance and recorded extracted engine SHA256 evidence")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    parser.add_argument("--expected-version", required=True)
    parser.add_argument("--adapter-revision", required=True)
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--acceptance-target", choices=("github-hosted", "windows11-ordinary"),
                        default="windows11-ordinary")
    args = parser.parse_args()
    check(args.executable, args.expected_version, args.adapter_revision, args.evidence, args.acceptance_target)


if __name__ == "__main__":
    main()
