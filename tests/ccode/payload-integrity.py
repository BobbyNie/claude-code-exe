"""Acceptance: a modified embedded original payload must fail integrity checks."""
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import threading
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


@contextmanager
def rejecting_gateway():
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            self.rfile.read(int(self.headers.get('Content-Length', '0')))
            self.server.requests.append(self.path.split('?', 1)[0])
            body = b'{"type":"error","error":{"type":"authentication_error","message":"test rejection"}}'
            self.send_response(401)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.requests = []
    server.url = f'http://127.0.0.1:{server.server_address[1]}'
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=5)


def failure_summary(exit_code, request_count, terminal, attempt):
    """Only exact neutral renderer lines; never retain engine text or URLs."""
    lines = set(terminal.splitlines())
    return {"attempt": attempt, "exit_code": exit_code, "request_count": request_count,
            "auth": b"[E_GATEWAY_AUTH: authentication failed]" in lines,
            "engine": bool(lines & {b"[E_ENGINE: turn failed]",
                b"[E_ENGINE: request failed]",
                b"[E_ENGINE: incomplete turn; check gateway and configuration]"}),
            "retry": b"[E_GATEWAY_RETRY: automatic retry refused]" in lines}


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
    changed = tamper_payload(source, payload, metadata['engineSha256'])
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
        # A valid original must be extracted unchanged; a changed cached copy
        # must be replaced before actual engine execution, not merely trusted by name.
        candidate.write_bytes(source)
        outside = root / 'outside-runtime'
        outside.mkdir()
        sentinel = outside / 'preserve.txt'
        sentinel.write_bytes(b'outside-runtime-preservation')
        junction = root / 'runtime'
        linked = subprocess.run(['cmd.exe', '/d', '/c', 'mklink', '/J', str(junction), str(outside)],
                                capture_output=True, timeout=10)
        assert linked.returncode == 0, 'Cannot establish runtime junction acceptance fixture'
        try:
            rejected = subprocess.run([str(candidate), '--data-dir', str(data), '--print',
                                       'integrity-test-only'], cwd=root, env=environment,
                                      input=b'', capture_output=True, timeout=30)
            assert rejected.returncode == 64, 'Runtime junction was not rejected'
            assert rejected.stdout == b'' and rejected.stderr.strip() == b'E_RUNTIME_PATH'
            assert list(outside.iterdir()) == [sentinel], 'Runtime junction target was modified'
            assert sentinel.read_bytes() == b'outside-runtime-preservation'
        finally:
            os.rmdir(junction)  # Remove the junction itself, never its target contents.
        runtime_directory = root / 'runtime' / metadata['engineSha256']
        runtime_directory.mkdir(parents=True)
        linked_candidate = runtime_directory / 'engine.new'
        os.link(sentinel, linked_candidate)
        try:
            rejected = subprocess.run([str(candidate), '--data-dir', str(data), '--print',
                                       'integrity-test-only'], cwd=root, env=environment,
                                      input=b'', capture_output=True, timeout=30)
            assert rejected.returncode == 64, 'Hard-linked extraction candidate was not rejected'
            assert rejected.stdout == b'' and rejected.stderr.strip() == b'E_RUNTIME_PATH'
            assert sentinel.read_bytes() == b'outside-runtime-preservation', 'Hard-link target changed'
            assert linked_candidate.read_bytes() == sentinel.read_bytes()
            assert not (runtime_directory / 'engine.exe').exists(), 'Engine was extracted through hard link'
        finally:
            linked_candidate.unlink()
        with rejecting_gateway() as gateway:
            environment['A_BASE_URL'] = gateway.url
            extracted = root / 'runtime' / metadata['engineSha256'] / 'engine.exe'
            for attempt in range(2):
                if attempt:
                    with extracted.open('r+b') as cached:
                        cached.seek(-1, 2)
                        value = cached.read(1)
                        cached.seek(-1, 2)
                        cached.write(bytes([value[0] ^ 1]))
                    assert hashlib.sha256(extracted.read_bytes()).hexdigest() != metadata['engineSha256']
                before = len(gateway.requests)
                turn = subprocess.run([str(candidate), '--data-dir', str(data), '--print',
                                       '--tools', '', 'integrity-test-only'], cwd=root,
                                      env=environment, input=b'', capture_output=True, timeout=60)
                assert turn.returncode != 0, 'Fixture authentication rejection unexpectedly succeeded'
                terminal = turn.stderr + turn.stdout
                summary = failure_summary(turn.returncode, len(gateway.requests) - before,
                                          terminal, attempt)
                assert summary['auth'], ('Engine did not reach auth fixture; ' +
                                         json.dumps(summary, sort_keys=True))
                assert b'test-only-integrity-token' not in terminal, 'Fixture token disclosed'
                assert b'integrity-test-only' not in terminal, 'Fixture prompt disclosed'
                assert gateway.requests[before:] == ['/v1/messages'], ('Expected one actual engine request; ' +
                    json.dumps(summary, sort_keys=True))
                restored = extracted.read_bytes()
                assert hashlib.sha256(restored).hexdigest() == metadata['engineSha256'], 'Extracted hash differs'
                assert restored == payload, 'Extracted bytes differ from original embedded payload'
                assert not (extracted.parent / 'engine.new').exists(), 'Extraction candidate left behind'
        assert executable.read_bytes() == source, 'Original build was modified'
    print('PASS: embedded tampering rejected; actual extracted payload and repaired cache match original hash and bytes')


if __name__ == '__main__':
    check(sys.argv[1])
