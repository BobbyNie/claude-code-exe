"""Audit public delivery names; never change package files or original binaries."""
from pathlib import Path, PurePosixPath
import stat
import argparse
import json
import zipfile
import hashlib


def _link(status):
    return stat.S_ISLNK(status.st_mode) or bool(getattr(status, 'st_file_attributes', 0) & 0x400)


def _policy(restricted_names, opaque_files):
    if not restricted_names or any(not isinstance(name, str) or not name.strip() for name in restricted_names):
        raise ValueError('E_PACKAGE_POLICY')
    opaque = set(opaque_files)
    if len(opaque) != len(opaque_files):
        raise ValueError('E_PACKAGE_POLICY')
    for name in opaque:
        if (not isinstance(name, str) or '\\' in name or ':' in name or
                any(part in ('', '.', '..') for part in name.split('/')) or
                PurePosixPath(name).suffix.casefold() not in ('.exe', '.dll')):
            raise ValueError('E_PACKAGE_POLICY')
    return [name.casefold() for name in restricted_names], opaque


def _text(contents, relative):
    encoding = 'utf-16' if contents.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig'
    text = contents.decode(encoding)
    if '\x00' in text:
        raise ValueError('E_PACKAGE_ENCODING')
    if PurePosixPath(relative).suffix.casefold() == '.json':
        # Scan decoded public keys/values as well as their literal spelling.
        text += '\n' + json.dumps(json.loads(text), ensure_ascii=False)
    return text.casefold()


def _fingerprint(stream, relative):
    digest = hashlib.sha256()
    size = 0
    while True:
        chunk = stream.read(1024 * 1024)
        if not chunk:
            break
        digest.update(chunk)
        size += len(chunk)
    return {'path': relative, 'size': size, 'sha256': digest.hexdigest()}


def scan_directory(root, restricted_names, opaque_files=()):
    names, opaque = _policy(restricted_names, opaque_files)
    root = Path(root)
    try:
        status = root.lstat()
        if _link(status) or not stat.S_ISDIR(status.st_mode):
            raise ValueError('E_PACKAGE_ROOT')
    except OSError:
        raise ValueError('E_PACKAGE_ROOT') from None
    findings, observed_opaque = [], set()
    files = []

    def finding(relative, code):
        findings.append({'path': relative, 'code': code})

    def walk(directory):
        try:
            children = sorted(directory.iterdir())
        except OSError:
            finding(directory.relative_to(root).as_posix(), 'E_PACKAGE_READ')
            return
        for path in children:
            relative = path.relative_to(root).as_posix()
            if any(name in path.name.casefold() for name in names):
                finding(relative, 'E_PACKAGE_NAME')
            try:
                status = path.lstat()
                if _link(status):
                    finding(relative, 'E_PACKAGE_LINK')
                elif stat.S_ISDIR(status.st_mode):
                    walk(path)
                elif not stat.S_ISREG(status.st_mode):
                    finding(relative, 'E_PACKAGE_TYPE')
                elif relative in opaque:
                    # Explicit binary-content exclusion, never an exemption for
                    # filenames, directories, settings, or required notices.
                    with path.open('rb') as binary:
                        files.append(_fingerprint(binary, relative))
                        binary.seek(0)
                        if binary.read(2) != b'MZ':
                            raise ValueError('E_PACKAGE_POLICY')
                    observed_opaque.add(relative)
                else:
                    try:
                        contents = path.read_bytes()
                        files.append({'path': relative, 'size': len(contents),
                                      'sha256': hashlib.sha256(contents).hexdigest()})
                        text = _text(contents, relative)
                    except (UnicodeError, ValueError):
                        finding(relative, 'E_PACKAGE_ENCODING')
                        continue
                    if any(name in text for name in names):
                        finding(relative, 'E_PACKAGE_PUBLIC_TEXT')
            except OSError:
                finding(relative, 'E_PACKAGE_READ')

    walk(root)
    if observed_opaque != opaque:
        raise ValueError('E_PACKAGE_POLICY')
    return _report(names, opaque, findings, files)


def _report(names, opaque, findings, files):
    return {'schema': 1, 'status': 'failed' if findings else 'passed',
            'scope': {'opaqueContents': sorted(opaque), 'parentDirectories': 'excluded',
                      'restrictedNames': sorted(set(names))},
            'findings': findings, 'files': sorted(files, key=lambda item: item['path'])}


def scan_archive(archive, restricted_names, opaque_files=()):
    names, opaque = _policy(restricted_names, opaque_files)
    findings, observed_opaque = [], set()
    files = []
    seen = set()
    archive = Path(archive)
    if any(name in archive.name.casefold() for name in names):
        findings.append({'path': archive.name, 'code': 'E_PACKAGE_NAME'})
    try:
        with archive.open('rb') as source:
            archive_hash = _fingerprint(source, archive.name)['sha256']
        with zipfile.ZipFile(archive) as bundle:
            for entry in sorted(bundle.infolist(), key=lambda entry: entry.filename):
                # ZipInfo.filename is normalized on Windows; validate the raw
                # archive spelling before any content is opened.
                relative = entry.orig_filename.rstrip('/')
                if ('\\' in relative or ':' in relative or
                        any(part in ('', '.', '..') for part in relative.split('/'))):
                    findings.append({'path': relative, 'code': 'E_PACKAGE_PATH'})
                    continue
                if relative.casefold() in seen:
                    findings.append({'path': relative, 'code': 'E_PACKAGE_COLLISION'})
                    continue
                seen.add(relative.casefold())
                if stat.S_ISLNK(entry.external_attr >> 16):
                    findings.append({'path': relative, 'code': 'E_PACKAGE_LINK'})
                    continue
                if any(name in relative.casefold() for name in names):
                    findings.append({'path': relative, 'code': 'E_PACKAGE_NAME'})
                if entry.is_dir():
                    continue
                if relative in opaque:
                    with bundle.open(entry) as binary:
                        files.append(_fingerprint(binary, relative))
                        binary.seek(0)
                        if binary.read(2) != b'MZ':
                            raise ValueError('E_PACKAGE_POLICY')
                    observed_opaque.add(relative)
                else:
                    try:
                        contents = bundle.read(entry)
                        files.append({'path': relative, 'size': len(contents),
                                      'sha256': hashlib.sha256(contents).hexdigest()})
                        text = _text(contents, relative)
                    except (UnicodeError, ValueError):
                        findings.append({'path': relative, 'code': 'E_PACKAGE_ENCODING'})
                        continue
                    if any(name in text for name in names):
                        findings.append({'path': relative, 'code': 'E_PACKAGE_PUBLIC_TEXT'})
    except (OSError, zipfile.BadZipFile, RuntimeError, NotImplementedError):
        raise ValueError('E_PACKAGE_ARCHIVE') from None
    if observed_opaque != opaque:
        raise ValueError('E_PACKAGE_POLICY')
    report = _report(names, opaque, findings, files)
    report['archiveSha256'] = archive_hash
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', help='Unpacked delivery directory; user-selected parent names are excluded')
    parser.add_argument('--archive', action='store_true', help='Scan ZIP members without extracting')
    parser.add_argument('--unpacked', help='With --archive, verify the corresponding unpacked directory')
    parser.add_argument('--restricted-name', action='append', required=True,
                        help='Case-insensitive prohibited public name; repeat for each name')
    parser.add_argument('--opaque', action='append', default=[],
                        help='Exact relative .exe/.dll path whose original contents are outside naming scope')
    options = parser.parse_args()
    if options.unpacked and not options.archive:
        parser.error('--unpacked requires --archive')
    try:
        scan = scan_archive if options.archive else scan_directory
        report = scan(options.root, options.restricted_name, options.opaque)
        if options.unpacked:
            unpacked = scan_directory(options.unpacked, options.restricted_name, options.opaque)
            matched = report['files'] == unpacked['files']
            passed = matched and report['status'] == unpacked['status'] == 'passed'
            report = {'schema': 1, 'status': 'passed' if passed else 'failed',
                      'comparison': 'matched' if matched else 'mismatched',
                      'archive': report, 'unpacked': unpacked}
    except ValueError as error:
        print(json.dumps({'schema': 1, 'status': 'error', 'code': str(error)}))
        return 2
    print(json.dumps(report, ensure_ascii=True, sort_keys=True))
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
