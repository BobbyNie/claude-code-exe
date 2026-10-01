"""Build an audited, allowlisted Windows 11 x64 enterprise delivery candidate."""
from pathlib import Path
from urllib.parse import urlsplit
import argparse
import hashlib
import json
import os
import re
import stat
import tempfile
import zipfile

import package_audit


HEX_40_OR_64 = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
HEX_64 = re.compile(r"^[0-9a-f]{64}$")
VERSION = re.compile(r"^[0-9A-Za-z](?:[0-9A-Za-z._-]{0,62}[0-9A-Za-z])?$")
WINDOWS_RESERVED = {
    "con", "prn", "aux", "nul",
    *(f"com{number}" for number in range(1, 10)),
    *(f"lpt{number}" for number in range(1, 10)),
}
BOUNDARY_DOCUMENT = {
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
            "NODE_TLS_REJECT_UNAUTHORIZED": "1",
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


class PackageBuildError(Exception):
    def __init__(self, code, *, audit=None):
        super().__init__(code)
        self.code = code
        self.audit = audit


def _sha256(contents):
    return hashlib.sha256(contents).hexdigest()


def _regular_bytes(path):
    path = Path(path)
    try:
        status = path.lstat()
        if stat.S_ISLNK(status.st_mode) or not stat.S_ISREG(status.st_mode):
            raise PackageBuildError("E_INPUT_TYPE")
        return path.read_bytes()
    except PackageBuildError:
        raise
    except OSError:
        raise PackageBuildError("E_INPUT_READ") from None


def _https(value):
    if not isinstance(value, str):
        return False
    parsed = urlsplit(value)
    return (parsed.scheme == "https" and bool(parsed.netloc) and
            parsed.username is None and parsed.password is None and not parsed.fragment)


def _unique_json_object(pairs):
    document = {}
    for key, value in pairs:
        if key in document:
            raise ValueError("Duplicate JSON key")
        document[key] = value
    return document


def _validated_provenance(path):
    try:
        raw = _regular_bytes(path)
        document = json.loads(raw.decode("utf-8-sig"), object_pairs_hook=_unique_json_object)
    except (UnicodeError, ValueError):
        raise PackageBuildError("E_PROVENANCE") from None
    required = {
        "schemaVersion", "packageName", "packageVersion", "platform", "architecture",
        "adapterRevision", "engineVersion", "engineSha256", "engineSize",
        "officialManifestUrl", "officialManifestSha256", "officialPayloadUrl",
    }
    if not isinstance(document, dict) or not required.issubset(document):
        raise PackageBuildError("E_PROVENANCE")
    adapter = str(document["adapterRevision"]).casefold()
    engine_hash = str(document["engineSha256"]).casefold()
    manifest_hash = str(document["officialManifestSha256"]).casefold()
    version = document["engineVersion"]
    size = document["engineSize"]
    if (document["schemaVersion"] != 1 or document["packageName"] != "ccode" or
            document["platform"] != "windows" or document["architecture"] != "x64" or
            not isinstance(document["packageVersion"], str) or not document["packageVersion"] or
            not isinstance(version, str) or not VERSION.fullmatch(version) or
            not HEX_40_OR_64.fullmatch(adapter) or not HEX_64.fullmatch(engine_hash) or
            not HEX_64.fullmatch(manifest_hash) or isinstance(size, bool) or
            not isinstance(size, int) or size <= 0 or
            not _https(document["officialManifestUrl"]) or
            not _https(document["officialPayloadUrl"])):
        raise PackageBuildError("E_PROVENANCE")
    return {
        "schemaVersion": 1,
        "packageName": "ccode",
        "packageVersion": document["packageVersion"],
        "adapterRevision": adapter,
        "engineVersion": version,
        "engineSha256": engine_hash,
        "engineSize": size,
        "officialManifestUrl": document["officialManifestUrl"],
        "officialManifestSha256": manifest_hash,
        "officialPayloadUrl": document["officialPayloadUrl"],
    }


def _validated_boundary(path):
    try:
        raw = _regular_bytes(path)
        document = json.loads(raw.decode("utf-8-sig"), object_pairs_hook=_unique_json_object)
    except (UnicodeError, ValueError):
        raise PackageBuildError("E_BOUNDARY") from None
    # Canonical JSON distinguishes booleans, integers and floats; Python
    # container equality incorrectly accepts True == 1 and False == 0.
    if json.dumps(document, sort_keys=True) != json.dumps(BOUNDARY_DOCUMENT, sort_keys=True):
        raise PackageBuildError("E_BOUNDARY")
    return document


def _notice_name(path):
    name = Path(path).name
    stem = name.split(".", 1)[0].casefold()
    if (not name or name in (".", "..") or name[-1] in " ." or
            any(character in name for character in '<>:"/\\|?*') or stem in WINDOWS_RESERVED):
        raise PackageBuildError("E_NOTICE_NAME")
    return name


def _entry(path, contents):
    return {"path": path, "size": len(contents), "sha256": _sha256(contents)}


def _write(path, contents):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(contents)


def _zip_directory(root, archive):
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for source in sorted(path for path in root.rglob("*") if path.is_file()):
            relative = source.relative_to(root).as_posix()
            info = zipfile.ZipInfo(relative, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            mode = 0o755 if relative == "ccode.exe" else 0o644
            info.external_attr = mode << 16
            info.flag_bits |= 0x800
            bundle.writestr(info, source.read_bytes(), compress_type=zipfile.ZIP_DEFLATED,
                            compresslevel=9)


def _audit(archive, unpacked, restricted_names):
    try:
        zipped = package_audit.scan_archive(archive, restricted_names, ["ccode.exe"])
        directory = package_audit.scan_directory(unpacked, restricted_names, ["ccode.exe"])
    except ValueError as error:
        raise PackageBuildError(str(error)) from None
    matched = zipped["files"] == directory["files"]
    passed = matched and zipped["status"] == directory["status"] == "passed"
    return {
        "schema": 1,
        "status": "passed" if passed else "failed",
        "comparison": "matched" if matched else "mismatched",
        "archive": zipped,
        "unpacked": directory,
    }


def build(executable, provenance_path, boundary_path, usage_path, notices, notice_hashes,
          restricted_names, output):
    output = Path(output)
    if output.exists() or output.is_symlink():
        raise PackageBuildError("E_OUTPUT_EXISTS")
    if not restricted_names or any(not isinstance(name, str) or not name.strip()
                                   for name in restricted_names):
        raise PackageBuildError("E_PACKAGE_POLICY")
    if not notices or len(notices) != len(notice_hashes):
        raise PackageBuildError("E_NOTICE_REQUIRED")

    executable_bytes = _regular_bytes(executable)
    if not executable_bytes.startswith(b"MZ"):
        raise PackageBuildError("E_EXECUTABLE")
    usage_bytes = _regular_bytes(usage_path)
    try:
        usage_text = usage_bytes.decode("utf-8-sig")
        if not usage_text.strip() or "\x00" in usage_text:
            raise UnicodeError
    except UnicodeError:
        raise PackageBuildError("E_USAGE") from None
    provenance = _validated_provenance(provenance_path)
    boundary = _validated_boundary(boundary_path)

    notice_entries = []
    notice_payloads = []
    observed_names = set()
    for source, expected in zip(notices, notice_hashes):
        expected = expected.casefold()
        if not HEX_64.fullmatch(expected):
            raise PackageBuildError("E_NOTICE_HASH")
        name = _notice_name(source)
        if name.casefold() in observed_names:
            raise PackageBuildError("E_NOTICE_NAME")
        observed_names.add(name.casefold())
        contents = _regular_bytes(source)
        if _sha256(contents) != expected:
            raise PackageBuildError("E_NOTICE_HASH")
        destination = f"notices/{name}"
        notice_payloads.append((destination, contents))
        notice_entries.append(_entry(destination, contents))

    payloads = [
        ("ccode.exe", executable_bytes),
        ("docs/usage.md", usage_bytes),
        *notice_payloads,
    ]
    file_entries = [_entry(path, contents) for path, contents in payloads]
    manifest = {
        "schemaVersion": 1,
        "packageName": "ccode-enterprise",
        "packageVersion": provenance["engineVersion"],
        "platform": "windows",
        "architecture": "x64",
        "minimumWindowsBuild": 22000,
        "executable": _entry("ccode.exe", executable_bytes),
        "provenance": provenance,
        "runtimeBoundary": boundary,
        "files": sorted(file_entries, key=lambda item: item["path"]),
        "notices": sorted(notice_entries, key=lambda item: item["path"]),
        "publicBoundary": {
            "opaqueContents": ["ccode.exe"],
            "scannedText": sorted(["docs/usage.md", "manifest.json",
                                   *(entry["path"] for entry in notice_entries)]),
            "parentDirectories": "excluded",
        },
        "excludedDynamicData": ["data/", "profile/", "runtime/", "sessions/", "temp/"],
        "redistributionApproval": "external-gate-not-asserted",
    }
    manifest_bytes = (json.dumps(manifest, ensure_ascii=True, sort_keys=True,
                                 separators=(",", ":")) + "\n").encode("utf-8")

    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".ccode-enterprise-", dir=output.parent) as temporary:
        temporary = Path(temporary)
        result_root = temporary / "result"
        unpacked = result_root / "unpacked"
        for destination, contents in payloads:
            _write(unpacked / destination, contents)
        _write(unpacked / "manifest.json", manifest_bytes)
        archive_name = f"ccode-enterprise-windows-x64-{provenance['engineVersion']}.zip"
        archive = result_root / archive_name
        _zip_directory(unpacked, archive)
        audit = _audit(archive, unpacked, restricted_names)
        if audit["status"] != "passed":
            raise PackageBuildError("E_PACKAGE_AUDIT", audit=audit)
        audit_name = "package-audit.json"
        _write(result_root / audit_name,
               (json.dumps(audit, ensure_ascii=True, sort_keys=True,
                           separators=(",", ":")) + "\n").encode("utf-8"))
        archive_hash = _sha256(archive.read_bytes())
        os.replace(result_root, output)

    return {
        "schema": 1,
        "status": "passed",
        "platform": "windows",
        "architecture": "x64",
        "minimumWindowsBuild": 22000,
        "archive": archive_name,
        "archiveSha256": archive_hash,
        "unpacked": "unpacked",
        "auditReport": audit_name,
        "audit": {"status": audit["status"], "comparison": audit["comparison"]},
        "redistributionApproval": "external-gate-not-asserted",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--provenance", required=True,
                        help="Verified JSON emitted by ccode.exe --package-manifest")
    parser.add_argument("--boundary", required=True,
                        help="Verified JSON emitted by ccode.exe --boundary-manifest")
    parser.add_argument("--usage", required=True)
    parser.add_argument("--notice", action="append", required=True,
                        help="Required notice to preserve byte-for-byte; repeat as needed")
    parser.add_argument("--notice-sha256", action="append", required=True,
                        help="Approved SHA-256 paired by order with --notice")
    parser.add_argument("--restricted-name", action="append", required=True,
                        help="Approved case-insensitive prohibited public name")
    parser.add_argument("--output", required=True,
                        help="New output directory; existing paths are rejected")
    options = parser.parse_args()
    try:
        report = build(
            options.executable,
            options.provenance,
            options.boundary,
            options.usage,
            options.notice,
            options.notice_sha256,
            options.restricted_name,
            options.output,
        )
    except PackageBuildError as error:
        report = {"schema": 1, "status": "error", "code": error.code}
        if error.audit is not None:
            report["audit"] = error.audit
        print(json.dumps(report, ensure_ascii=True, sort_keys=True))
        return 2
    print(json.dumps(report, ensure_ascii=True, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
