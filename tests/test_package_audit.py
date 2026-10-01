"""Public delivery names are audited without rewriting original binary contents."""
import importlib.util
import hashlib
import json
import subprocess
import sys
import zipfile
import stat
from pathlib import Path
import tempfile
import unittest
from unittest import mock

MODULE = Path(__file__).resolve().parents[1] / 'scripts/ccode/package_audit.py'


class PackageAuditTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('package_audit', MODULE)
        self.audit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.audit)

    def test_recursive_public_names_and_text_case_insensitive_without_parent_scope(self):
        with tempfile.TemporaryDirectory(prefix='Restricted-parent-') as temporary:
            root = Path(temporary)
            (root / 'docs').mkdir()
            (root / 'docs/usage.md').write_text('Neutral help', encoding='utf-8')
            self.assertEqual(self.audit.scan_directory(root, ['restricted'])['status'], 'passed')
            (root / 'docs/usage.md').write_text('Uses ReStRiCtEd settings', encoding='utf-8')
            (root / 'Restricted-old').mkdir()
            (root / 'Restricted-old/leftover.txt').write_text('neutral', encoding='utf-8')
            report = self.audit.scan_directory(root, ['restricted'])
            self.assertEqual(report['status'], 'failed')
            self.assertIn({'path': 'docs/usage.md', 'code': 'E_PACKAGE_PUBLIC_TEXT'}, report['findings'])
            self.assertIn({'path': 'Restricted-old', 'code': 'E_PACKAGE_NAME'}, report['findings'])

    def test_explicit_original_binary_contents_are_not_rewritten_or_claimed_name_free(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            engine = root / 'ccode.exe'
            engine.write_bytes(b'MZ\x00\xffRestricted original payload')
            before = hashlib.sha256(engine.read_bytes()).hexdigest()
            report = self.audit.scan_directory(root, ['restricted'], opaque_files=['ccode.exe'])
            self.assertEqual(report['status'], 'passed')
            self.assertEqual(report['scope']['opaqueContents'], ['ccode.exe'])
            self.assertEqual(report['scope']['parentDirectories'], 'excluded')
            self.assertEqual(hashlib.sha256(engine.read_bytes()).hexdigest(), before)
            rejected = self.audit.scan_directory(root, ['restricted'])
            self.assertIn({'path': 'ccode.exe', 'code': 'E_PACKAGE_ENCODING'}, rejected['findings'])
            notice = root / 'NOTICE.txt'
            notice.write_text('Required Restricted attribution', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, '^E_PACKAGE_POLICY$'):
                self.audit.scan_directory(root, ['restricted'], opaque_files=['NOTICE.txt'])
            report = self.audit.scan_directory(root, ['restricted'], opaque_files=['ccode.exe'])
            self.assertIn({'path': 'NOTICE.txt', 'code': 'E_PACKAGE_PUBLIC_TEXT'}, report['findings'])
            self.assertEqual(notice.read_text(), 'Required Restricted attribution')

    def test_utf16_public_configuration_is_scanned_and_binary_text_is_not_silently_skipped(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'settings.json').write_bytes('{"name":"ReStRiCtEd"}'.encode('utf-16'))
            (root / 'opaque.dat').write_bytes(b'\x00\x01')
            report = self.audit.scan_directory(root, ['restricted'])
            self.assertIn({'path': 'settings.json', 'code': 'E_PACKAGE_PUBLIC_TEXT'}, report['findings'])
            self.assertIn({'path': 'opaque.dat', 'code': 'E_PACKAGE_ENCODING'}, report['findings'])

    def test_missing_root_empty_policy_and_invalid_binary_exclusions_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'ccode.exe').write_bytes(b'MZpayload')
            (root / 'fake.dll').write_text('not a PE payload')
            for names, opaque in [([], []), ([''], []), (['  '], []),
                                  (['restricted'], ['missing.exe']),
                                  (['restricted'], ['../ccode.exe']),
                                  (['restricted'], ['/ccode.exe']),
                                  (['restricted'], ['fake.dll']),
                                  (['restricted'], ['ccode.exe', 'ccode.exe'])]:
                with self.subTest(names=names, opaque=opaque):
                    with self.assertRaisesRegex(ValueError, '^E_PACKAGE_POLICY$'):
                        self.audit.scan_directory(root, names, opaque)
            with self.assertRaisesRegex(ValueError, '^E_PACKAGE_ROOT$'):
                self.audit.scan_directory(root / 'missing', ['restricted'])

    def test_links_are_rejected_without_reading_outside_the_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            root = parent / 'package'
            root.mkdir()
            private = parent / 'private.txt'
            private.write_text('Restricted outside content', encoding='utf-8')
            try:
                (root / 'notice.txt').symlink_to(private)
            except OSError:
                self.skipTest('Creating a real symlink is not permitted on this account')
            report = self.audit.scan_directory(root, ['restricted'])
            self.assertEqual(report['findings'], [{'path': 'notice.txt', 'code': 'E_PACKAGE_LINK'}])

    def test_cli_reports_scope_and_failure_exit_without_modifying_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            notice = root / 'NOTICE.txt'
            notice.write_text('Required Restricted attribution', encoding='utf-8')
            command = [sys.executable, str(MODULE), str(root), '--restricted-name', 'restricted']
            failed = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(failed.returncode, 1)
            report = json.loads(failed.stdout)
            self.assertEqual(report['status'], 'failed')
            self.assertEqual(report['scope']['restrictedNames'], ['restricted'])
            self.assertEqual(notice.read_text(), 'Required Restricted attribution')
            notice.write_text('Neutral attribution', encoding='utf-8')
            passed = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(passed.returncode, 0)
            self.assertEqual(json.loads(passed.stdout)['status'], 'passed')

    def test_zip_and_unpacked_delivery_report_the_same_public_content_conflict(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / 'package.zip'
            unpacked = root / 'unpacked'
            (unpacked / 'docs').mkdir(parents=True)
            (unpacked / 'docs/usage.md').write_bytes('Restricted setting'.encode('utf-16'))
            (unpacked / 'ccode.exe').write_bytes(b'MZ\x00Restricted original')
            with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as bundle:
                for name in ('ccode.exe', 'docs/usage.md'):
                    bundle.write(unpacked / name, name)
            options = (['restricted'], ['ccode.exe'])
            directory_report = self.audit.scan_directory(unpacked, *options)
            archive_report = self.audit.scan_archive(archive, *options)
            self.assertEqual(archive_report['status'], 'failed')
            self.assertEqual(archive_report['findings'], directory_report['findings'])
            self.assertEqual(archive_report['scope'], directory_report['scope'])
            result = subprocess.run([sys.executable, str(MODULE), str(archive),
                                     '--archive', '--restricted-name', 'restricted',
                                     '--opaque', 'ccode.exe'], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(json.loads(result.stdout)['findings'], archive_report['findings'])

    def test_archive_paths_links_and_case_collisions_are_rejected_without_extraction(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / 'package.zip'
            with zipfile.ZipFile(archive, 'w') as bundle:
                for name in ('../outside.txt', '/absolute.txt', 'drive:stream', 'folder\\escape.txt',
                             'docs/usage.md', 'docs/USAGE.md'):
                    # ZipInfo normalizes backslashes on Windows at construction.
                    # Assign the raw member spelling afterwards so the hostile
                    # archive is identical on every host.
                    entry = zipfile.ZipInfo('placeholder')
                    entry.filename = name
                    bundle.writestr(entry, 'neutral')
                self.assertEqual(bundle.namelist(),
                                 ['../outside.txt', '/absolute.txt', 'drive:stream',
                                  'folder\\escape.txt', 'docs/usage.md', 'docs/USAGE.md'])
                link = zipfile.ZipInfo('linked.txt')
                link.create_system = 3
                link.external_attr = (stat.S_IFLNK | 0o777) << 16
                bundle.writestr(link, 'outside.txt')
            report = self.audit.scan_archive(archive, ['restricted'])
            self.assertEqual(report['status'], 'failed')
            codes = [item['code'] for item in report['findings']]
            self.assertEqual(codes.count('E_PACKAGE_PATH'), 4)
            self.assertIn('E_PACKAGE_COLLISION', codes)
            self.assertIn('E_PACKAGE_LINK', codes)
            self.assertEqual(list(root.iterdir()), [archive])

    def test_archive_raw_backslash_is_rejected_after_reader_normalization(self):
        with tempfile.TemporaryDirectory() as temporary:
            archive = Path(temporary) / 'package.zip'
            with zipfile.ZipFile(archive, 'w') as bundle:
                entry = zipfile.ZipInfo('placeholder')
                entry.filename = 'folder\\escape.txt'
                bundle.writestr(entry, 'neutral')
            original = zipfile.ZipInfo

            class WindowsReaderInfo(original):
                def __init__(self, filename='NoName', *args, **kwargs):
                    super().__init__(filename, *args, **kwargs)
                    self.filename = self.filename.replace('\\', '/')

            with mock.patch.object(zipfile, 'ZipInfo', WindowsReaderInfo):
                report = self.audit.scan_archive(archive, ['restricted'])
            self.assertEqual(report['findings'],
                             [{'path': 'folder\\escape.txt', 'code': 'E_PACKAGE_PATH'}])
            self.assertEqual(report['files'], [])

    def test_invalid_archives_fail_closed_with_neutral_cli_error(self):
        with tempfile.TemporaryDirectory(prefix='private-location-') as temporary:
            archive = Path(temporary) / 'bad.zip'
            archive.write_bytes(b'not a ZIP: private detail')
            result = subprocess.run([sys.executable, str(MODULE), str(archive), '--archive',
                                     '--restricted-name', 'restricted'], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(json.loads(result.stdout),
                             {'schema': 1, 'status': 'error', 'code': 'E_PACKAGE_ARCHIVE'})
            self.assertEqual(result.stderr, '')

    def test_json_escape_cannot_hide_a_public_configuration_name(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            settings = root / 'settings.json'
            contents = b'{"engine":"Re\\u0073tricted"}'
            settings.write_bytes(contents)
            report = self.audit.scan_directory(root, ['restricted'])
            self.assertIn({'path': 'settings.json', 'code': 'E_PACKAGE_PUBLIC_TEXT'}, report['findings'])
            self.assertEqual(settings.read_bytes(), contents)
            archive = root.parent / (root.name + '.zip')
            try:
                with zipfile.ZipFile(archive, 'w') as bundle:
                    bundle.writestr('settings.json', contents)
                zipped = self.audit.scan_archive(archive, ['restricted'])
                self.assertEqual(zipped['findings'], report['findings'])
            finally:
                archive.unlink(missing_ok=True)

    def test_report_records_hashes_of_text_and_opaque_payload_in_both_forms(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            unpacked = root / 'package'
            unpacked.mkdir()
            contents = {'ccode.exe': b'MZ\x00original', 'help.txt': b'neutral help'}
            archive = root / 'package.zip'
            with zipfile.ZipFile(archive, 'w') as bundle:
                for name, data in contents.items():
                    (unpacked / name).write_bytes(data)
                    bundle.writestr(name, data)
            expected = [{'path': name, 'size': len(data),
                         'sha256': hashlib.sha256(data).hexdigest()}
                        for name, data in sorted(contents.items())]
            directory = self.audit.scan_directory(unpacked, ['restricted'], ['ccode.exe'])
            zipped = self.audit.scan_archive(archive, ['restricted'], ['ccode.exe'])
            self.assertEqual(directory['files'], expected)
            self.assertEqual(zipped['files'], expected)
            self.assertEqual(zipped['archiveSha256'], hashlib.sha256(archive.read_bytes()).hexdigest())
            (unpacked / 'ccode.exe').write_bytes(b'MZchanged')
            changed = self.audit.scan_directory(unpacked, ['restricted'], ['ccode.exe'])
            self.assertNotEqual(changed['files'], directory['files'])

    def test_cli_pairs_archive_with_unpacked_contents_and_rejects_mismatch(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            unpacked = root / 'package'
            unpacked.mkdir()
            payload = b'MZoriginal'
            (unpacked / 'ccode.exe').write_bytes(payload)
            archive = root / 'package.zip'
            with zipfile.ZipFile(archive, 'w') as bundle:
                bundle.writestr('ccode.exe', payload)
            command = [sys.executable, str(MODULE), str(archive), '--archive',
                       '--unpacked', str(unpacked), '--restricted-name', 'restricted',
                       '--opaque', 'ccode.exe']
            paired = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(paired.returncode, 0, paired.stderr)
            report = json.loads(paired.stdout)
            self.assertEqual(report['status'], 'passed')
            self.assertEqual(report['comparison'], 'matched')
            self.assertEqual(report['archive']['files'], report['unpacked']['files'])
            (unpacked / 'ccode.exe').write_bytes(b'MZchanged')
            mismatch = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(mismatch.returncode, 1)
            report = json.loads(mismatch.stdout)
            self.assertEqual(report['comparison'], 'mismatched')
            self.assertEqual(report['status'], 'failed')
            self.assertEqual(report['archive']['status'], 'passed')
            self.assertEqual(report['unpacked']['status'], 'passed')
            (unpacked / 'ccode.exe').write_bytes(payload)
            (unpacked / 'leftover.txt').write_bytes(b'neutral leftover')
            leftover = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(leftover.returncode, 1)
            self.assertEqual(json.loads(leftover.stdout)['comparison'], 'mismatched')
            (unpacked / 'leftover.txt').unlink()
            (unpacked / 'notice.txt').write_bytes(b'Restricted required notice')
            with zipfile.ZipFile(archive, 'a') as bundle:
                bundle.writestr('notice.txt', b'Restricted required notice')
            conflict = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(conflict.returncode, 1)
            report = json.loads(conflict.stdout)
            self.assertEqual(report['comparison'], 'matched')
            self.assertEqual(report['status'], 'failed')
            self.assertEqual(report['archive']['status'], 'failed')
            self.assertEqual(report['unpacked']['status'], 'failed')

    def test_archive_delivery_filename_is_not_exempted_as_a_user_parent(self):
        with tempfile.TemporaryDirectory() as temporary:
            archive = Path(temporary) / 'Restricted-release.zip'
            with zipfile.ZipFile(archive, 'w') as bundle:
                bundle.writestr('docs/usage.md', 'neutral help')
            report = self.audit.scan_archive(archive, ['restricted'])
            self.assertEqual(report['findings'],
                             [{'path': 'Restricted-release.zip', 'code': 'E_PACKAGE_NAME'}])


if __name__ == '__main__':
    unittest.main()
