"""Revalidate and compare complete Windows 11 x64 enterprise candidates."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import stat
import subprocess

import package_audit
from build_enterprise_package import BOUNDARY_DOCUMENT, _unique_json_object, _https


HEX_64 = re.compile(r"^[0-9a-f]{64}$")
HEX_40_OR_64 = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")


class LifecycleError(Exception):
    def __init__(self, code):
        super().__init__(code)
        self.code = code


def _sha256(path):
    digest = hashlib.sha256()
    try:
        with Path(path).open("rb") as source:
            while chunk := source.read(1024 * 1024):
                digest.update(chunk)
    except OSError:
        raise LifecycleError("E_LIFECYCLE_READ") from None
    return digest.hexdigest()


def _json(path, code="E_LIFECYCLE_FORMAT"):
    try:
        status = Path(path).lstat()
        if stat.S_ISLNK(status.st_mode) or not stat.S_ISREG(status.st_mode):
            raise LifecycleError(code)
        return json.loads(Path(path).read_text(encoding="utf-8-sig"),
                          object_pairs_hook=_unique_json_object)
    except LifecycleError:
        raise
    except (OSError, UnicodeError, ValueError):
        raise LifecycleError(code) from None


def _root(path):
    root = Path(path)
    try:
        status = root.lstat()
        if stat.S_ISLNK(status.st_mode) or not stat.S_ISDIR(status.st_mode):
            raise LifecycleError("E_LIFECYCLE_ROOT")
        children = list(root.iterdir())
    except LifecycleError:
        raise
    except OSError:
        raise LifecycleError("E_LIFECYCLE_ROOT") from None
    archives = [item for item in children if item.suffix.casefold() == ".zip"]
    expected_names = {"unpacked", "package-audit.json", *(item.name for item in archives)}
    if len(archives) != 1 or {item.name for item in children} != expected_names:
        raise LifecycleError("E_LIFECYCLE_LAYOUT")
    return root, archives[0]


def _manifest_files(unpacked, manifest):
    required = {
        "schemaVersion", "packageName", "packageVersion", "platform", "architecture",
        "minimumWindowsBuild", "executable", "provenance", "runtimeBoundary", "files",
        "notices", "publicBoundary", "excludedDynamicData", "redistributionApproval",
    }
    if (not isinstance(manifest, dict) or not required.issubset(manifest) or
            manifest["schemaVersion"] != 1 or manifest["packageName"] != "ccode-enterprise" or
            manifest["platform"] != "windows" or manifest["architecture"] != "x64" or
            manifest["minimumWindowsBuild"] != 22000 or
            manifest["redistributionApproval"] != "external-gate-not-asserted" or
            manifest["excludedDynamicData"] !=
            ["data/", "profile/", "runtime/", "sessions/", "temp/"] or
            manifest.get("publicBoundary", {}).get("opaqueContents") != ["ccode.exe"]):
        raise LifecycleError("E_LIFECYCLE_MANIFEST")
    provenance = manifest["provenance"]
    required_provenance = {
        "schemaVersion", "packageName", "packageVersion", "adapterRevision", "engineVersion",
        "engineSha256", "engineSize", "officialManifestUrl", "officialManifestSha256",
        "officialPayloadUrl",
    }
    if (not isinstance(provenance, dict) or set(provenance) != required_provenance or
            provenance["schemaVersion"] != 1 or provenance["packageName"] != "ccode" or
            not isinstance(provenance["packageVersion"], str) or not provenance["packageVersion"] or
            provenance["engineVersion"] != manifest["packageVersion"] or
            not HEX_40_OR_64.fullmatch(str(provenance["adapterRevision"])) or
            not HEX_64.fullmatch(str(provenance["engineSha256"])) or
            not HEX_64.fullmatch(str(provenance["officialManifestSha256"])) or
            isinstance(provenance["engineSize"], bool) or not isinstance(provenance["engineSize"], int) or
            provenance["engineSize"] <= 0 or
            json.dumps(manifest["runtimeBoundary"], sort_keys=True) !=
            json.dumps(BOUNDARY_DOCUMENT, sort_keys=True)):
        raise LifecycleError("E_LIFECYCLE_MANIFEST")
    try:
        valid_sources = (_https(provenance["officialManifestUrl"]) and
                         _https(provenance["officialPayloadUrl"]))
    except ValueError:
        valid_sources = False
    if not valid_sources:
        raise LifecycleError("E_LIFECYCLE_MANIFEST")
    entries = manifest["files"]
    notices = manifest["notices"]
    if (not isinstance(entries, list) or not isinstance(notices, list) or not notices or
            any(not isinstance(entry, dict) for entry in notices)):
        raise LifecycleError("E_LIFECYCLE_MANIFEST")
    by_path = {}
    for entry in entries:
        if (not isinstance(entry, dict) or set(entry) != {"path", "size", "sha256"} or
                not isinstance(entry["path"], str) or entry["path"] in by_path or
                isinstance(entry["size"], bool) or not isinstance(entry["size"], int) or entry["size"] < 0 or
                not isinstance(entry["sha256"], str) or not HEX_64.fullmatch(entry["sha256"])):
            raise LifecycleError("E_LIFECYCLE_MANIFEST")
        by_path[entry["path"]] = entry
    notice_paths = [entry.get("path") for entry in notices]
    if (any(not isinstance(path, str) for path in notice_paths) or
            set(by_path) != {"ccode.exe", "docs/usage.md", *notice_paths}):
        raise LifecycleError("E_LIFECYCLE_MANIFEST")
    if manifest["executable"] != by_path["ccode.exe"]:
        raise LifecycleError("E_LIFECYCLE_MANIFEST")
    if sorted(notices, key=lambda entry: entry.get("path", "")) != notices:
        raise LifecycleError("E_LIFECYCLE_MANIFEST")
    for notice in notices:
        if notice != by_path.get(notice.get("path")) or not notice["path"].startswith("notices/"):
            raise LifecycleError("E_LIFECYCLE_MANIFEST")
    for relative, entry in by_path.items():
        source = unpacked / relative
        try:
            status = source.lstat()
            if stat.S_ISLNK(status.st_mode) or not stat.S_ISREG(status.st_mode):
                raise LifecycleError("E_LIFECYCLE_MANIFEST")
            if status.st_size != entry["size"] or _sha256(source) != entry["sha256"]:
                raise LifecycleError("E_LIFECYCLE_MANIFEST")
        except OSError:
            raise LifecycleError("E_LIFECYCLE_MANIFEST") from None
    return by_path


def inspect_candidate(path):
    root, archive = _root(path)
    unpacked = root / "unpacked"
    saved = _json(root / "package-audit.json", "E_LIFECYCLE_AUDIT")
    if (not isinstance(saved, dict) or saved.get("schema") != 1 or
            saved.get("status") != "passed" or saved.get("comparison") != "matched"):
        raise LifecycleError("E_LIFECYCLE_AUDIT")
    try:
        restricted_archive = saved["archive"]["scope"]["restrictedNames"]
        restricted_unpacked = saved["unpacked"]["scope"]["restrictedNames"]
        opaque_archive = saved["archive"]["scope"]["opaqueContents"]
        opaque_unpacked = saved["unpacked"]["scope"]["opaqueContents"]
    except (KeyError, TypeError):
        raise LifecycleError("E_LIFECYCLE_AUDIT") from None
    if (restricted_archive != restricted_unpacked or not restricted_archive or
            opaque_archive != ["ccode.exe"] or opaque_unpacked != ["ccode.exe"]):
        raise LifecycleError("E_LIFECYCLE_AUDIT")
    try:
        actual_archive = package_audit.scan_archive(archive, restricted_archive, ["ccode.exe"])
        actual_unpacked = package_audit.scan_directory(unpacked, restricted_archive, ["ccode.exe"])
    except ValueError:
        raise LifecycleError("E_LIFECYCLE_AUDIT") from None
    if (actual_archive["status"] != "passed" or actual_unpacked["status"] != "passed" or
            actual_archive["files"] != actual_unpacked["files"] or
            actual_archive["files"] != saved.get("archive", {}).get("files") or
            actual_unpacked["files"] != saved.get("unpacked", {}).get("files") or
            actual_archive.get("archiveSha256") != saved.get("archive", {}).get("archiveSha256")):
        raise LifecycleError("E_LIFECYCLE_AUDIT")
    manifest = _json(unpacked / "manifest.json", "E_LIFECYCLE_MANIFEST")
    _manifest_files(unpacked, manifest)
    expected_archive = f"ccode-enterprise-windows-x64-{manifest['packageVersion']}.zip"
    expected_paths = {"ccode.exe", "docs/usage.md", "manifest.json",
                      *(entry["path"] for entry in manifest["notices"])}
    if archive.name != expected_archive or {entry["path"] for entry in actual_unpacked["files"]} != expected_paths:
        raise LifecycleError("E_LIFECYCLE_LAYOUT")
    return {
        "schema": 1,
        "status": "passed",
        "platform": "windows",
        "architecture": "x64",
        "minimumWindowsBuild": 22000,
        "archive": archive.name,
        "archiveSha256": actual_archive["archiveSha256"],
        "unpacked": "unpacked",
        "auditReport": "package-audit.json",
        "restrictedNames": restricted_archive,
        "usagePath": "docs/usage.md",
        "noticePaths": [entry["path"] for entry in manifest["notices"]],
        "unpackedFiles": actual_unpacked["files"],
        "manifest": manifest,
    }


def inspect_signed_candidate(path, signature, public_key, trusted_pin, node="node"):
    report = inspect_candidate(path)
    verifier = Path(__file__).with_name("verify-manifest-signature.mjs")
    try:
        completed = subprocess.run([node, str(verifier),
            str(Path(path) / "unpacked/manifest.json"), str(signature), str(public_key),
            trusted_pin], capture_output=True, text=True, timeout=15)
        evidence = json.loads(completed.stdout, object_pairs_hook=_unique_json_object)
        manifest_entry = next(entry for entry in report["unpackedFiles"]
                              if entry["path"] == "manifest.json")
        expected = {"schema": 1, "status": "passed",
                    "manifestSha256": manifest_entry["sha256"]}
        if completed.returncode != 0 or completed.stderr or evidence != expected:
            raise LifecycleError("E_LIFECYCLE_SIGNATURE")
    except (OSError, ValueError, subprocess.TimeoutExpired, StopIteration):
        raise LifecycleError("E_LIFECYCLE_SIGNATURE") from None
    report["signatureVerification"] = "passed"
    report["signedManifestSha256"] = manifest_entry["sha256"]
    return report


def compare_candidates(original, repacked):
    first = inspect_candidate(original)
    second = inspect_candidate(repacked)
    matched = (first["manifest"] == second["manifest"] and
               first["restrictedNames"] == second["restrictedNames"] and
               first["unpackedFiles"] == second["unpackedFiles"])
    if not matched:
        raise LifecycleError("E_LIFECYCLE_REPACK")
    return {
        "schema": 1,
        "status": "passed",
        "comparison": "matched",
        "archiveBytesIdentical": first["archiveSha256"] == second["archiveSha256"],
        "originalArchiveSha256": first["archiveSha256"],
        "repackedArchiveSha256": second["archiveSha256"],
        "unpackedFiles": first["unpackedFiles"],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    inspect = commands.add_parser("inspect")
    inspect.add_argument("--candidate-root", required=True)
    signed = commands.add_parser("inspect-signed")
    signed.add_argument("--candidate-root", required=True)
    signed.add_argument("--signature", required=True)
    signed.add_argument("--public-key", required=True)
    signed.add_argument("--trusted-pin", required=True)
    signed.add_argument("--node", default="node")
    compare = commands.add_parser("compare")
    compare.add_argument("--original", required=True)
    compare.add_argument("--repacked", required=True)
    options = parser.parse_args()
    try:
        if options.command == "inspect-signed":
            report = inspect_signed_candidate(options.candidate_root, options.signature,
                options.public_key, options.trusted_pin, options.node)
        elif options.command == "inspect":
            report = inspect_candidate(options.candidate_root)
        else:
            report = compare_candidates(options.original, options.repacked)
    except LifecycleError as error:
        print(json.dumps({"schema": 1, "status": "error", "code": error.code}, sort_keys=True))
        return 2
    print(json.dumps(report, ensure_ascii=True, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
