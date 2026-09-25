"""Acceptance: a modified embedded original payload must fail integrity checks."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def tamper_payload(image, payload, expected_sha256):
    if len(payload) < 3 or not payload.startswith(b'MZ'):
        raise ValueError('Invalid fixture payload')
    if hashlib.sha256(payload).hexdigest() != expected_sha256:
        raise ValueError('Fixture payload checksum mismatch')
    offset = image.find(payload)
    if offset < 0 or image.find(payload, offset + 1) >= 0:
        raise ValueError('Fixture payload is not unique')
    changed = bytearray(image)
    changed[offset + len(payload) - 1] ^= 1
    return changed


def resources(executable):
    # Load only as data: never execute candidate entry points to inspect resources.
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
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
    module = kernel.LoadLibraryExW(str(executable), None, 2)
    if not module:
        raise RuntimeError('Unable to load fixture resources')
    try:
        result = []
        for identifier in (101, 102):
            resource = kernel.FindResourceW(module, identifier, 10)
            if not resource:
                raise RuntimeError('Missing fixture resource')
            size = kernel.SizeofResource(module, resource)
            loaded = kernel.LoadResource(module, resource)
            address = kernel.LockResource(loaded) if loaded else None
            if not size or not address:
                raise RuntimeError('Unreadable fixture resource')
            result.append(ctypes.string_at(address, size))
        return result
    finally:
        kernel.FreeLibrary(module)


def check(executable):
    if sys.platform != 'win32':
        raise RuntimeError('Windows acceptance requires Windows')
    executable = Path(executable).resolve(strict=True)
    payload, metadata_bytes = resources(executable)
    metadata = json.loads(metadata_bytes.decode('utf-8-sig'))
    source = executable.read_bytes()
    changed = tamper_payload(source, payload, metadata['sha256'])
    with tempfile.TemporaryDirectory(prefix='ccode-integrity-') as temporary:
        root = Path(temporary)
        candidate = root / 'ccode.exe'
        candidate.write_bytes(source)
        baseline = subprocess.run([str(candidate), '--ccode-self-test'], cwd=root,
                                  capture_output=True, timeout=30)
        assert baseline.returncode == 0, 'Original self-test failed'
        candidate.write_bytes(changed)
        result = subprocess.run([str(candidate), '--ccode-self-test'], cwd=root,
                                capture_output=True, timeout=30)
        assert result.returncode == 64, 'Modified payload did not fail closed'
        assert result.stdout == b'', 'Unexpected self-test output'
        assert result.stderr.strip() == b'E_CHECKSUM', 'Missing neutral checksum diagnostic'
        environment = {key: value for key, value in os.environ.items()
                       if not key.upper().startswith(('A_', 'ANTHROPIC_', 'CLAUDE_', 'CCODE_'))}
        environment.update(A_AUTH_TOKEN='test-only-integrity-token',
                           A_BASE_URL='http://127.0.0.1:1')
        data = root / 'isolated-data'
        startup = subprocess.run([str(candidate), '--data-dir', str(data), '--print',
                                  'integrity-test-only'], cwd=root, env=environment,
                                 capture_output=True, timeout=30)
        assert startup.returncode == 64, 'Normal startup accepted modified payload'
        assert startup.stdout == b'', 'Modified candidate produced turn output'
        assert startup.stderr.strip() == b'E_CHECKSUM', 'Normal startup missed integrity check'
        assert not list(root.rglob('engine.exe')), 'Modified payload was extracted'
        assert not list(root.rglob('*.jsonl')), 'Modified payload produced session history'
        assert executable.read_bytes() == source, 'Original build was modified'
    print('PASS: original embedded payload hash verified; tampered payload rejected by self-test and normal startup')


if __name__ == '__main__':
    check(sys.argv[1])
