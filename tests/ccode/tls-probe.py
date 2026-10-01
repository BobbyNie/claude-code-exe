"""Privacy-safe structural diagnostics; not an acceptance substitute."""
import hashlib
import importlib.util
import json
import os
import ssl
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def summarize_events(events):
    evidence = dict.fromkeys(('assistant_api_error', 'assistant_authentication_failed',
        'retry_event', 'error_result', 'structured_tls_code'), False)
    tls_codes = {'CERT_HAS_EXPIRED', 'DEPTH_ZERO_SELF_SIGNED_CERT',
        'SELF_SIGNED_CERT_IN_CHAIN', 'UNABLE_TO_VERIFY_LEAF_SIGNATURE',
        'UNABLE_TO_GET_ISSUER_CERT_LOCALLY', 'ERR_TLS_CERT_ALTNAME_INVALID'}
    for event in events:
        if not isinstance(event, dict):
            continue
        kind = event.get('type')
        if kind == 'assistant':
            evidence['assistant_api_error'] |= event.get('error') == 'api_error'
            evidence['assistant_authentication_failed'] |= event.get('error') == 'authentication_failed'
        if kind == 'system':
            evidence['retry_event'] |= event.get('subtype') == 'api_retry'
        if kind == 'result':
            evidence['error_result'] |= event.get('is_error') is True
        if kind in ('assistant', 'system', 'result'):
            error = event.get('error')
            if isinstance(error, dict) and isinstance(error.get('code'), str):
                evidence['structured_tls_code'] |= error['code'] in tls_codes
    return evidence


def summarize_result_shape(events):
    """Report only fixed schema flags, never result errors or unknown subtype text."""
    flags = dict.fromkeys(('execution_error', 'max_turns_error', 'errors_array',
                           'string_error_entry', 'object_error_entry', 'success_subtype',
                           'error_subtype', 'result_text', 'error_text', 'error_object'), False)
    for event in events:
        if not isinstance(event, dict) or event.get('type') != 'result':
            continue
        flags['success_subtype'] |= event.get('subtype') == 'success'
        flags['error_subtype'] |= event.get('subtype') == 'error'
        flags['result_text'] |= isinstance(event.get('result'), str)
        flags['error_text'] |= isinstance(event.get('error'), str)
        flags['error_object'] |= isinstance(event.get('error'), dict)
        flags['execution_error'] |= event.get('subtype') == 'error_during_execution'
        flags['max_turns_error'] |= event.get('subtype') == 'error_max_turns'
        errors = event.get('errors')
        if isinstance(errors, list):
            flags['errors_array'] = True
            flags['string_error_entry'] |= any(isinstance(error, str) for error in errors)
            flags['object_error_entry'] |= any(isinstance(error, dict) for error in errors)
    return flags


def summarize_failure_text(events):
    """Investigation hints only; reflected text never proves TLS rejection."""
    flags = {"certificate_text": False, "connection_error_text": False}
    for event in events:
        if (not isinstance(event, dict) or event.get("type") != "result" or
                event.get("is_error") is not True):
            continue
        texts = [event.get("result"), event.get("error")]
        errors = event.get("errors")
        if isinstance(errors, list):
            texts.extend(errors)
        for text in texts:
            if not isinstance(text, str):
                continue
            normalized = text.lower()
            flags["certificate_text"] |= ("certificate" in normalized or
                "cert_has_expired" in normalized or "self_signed_cert" in normalized)
            flags["connection_error_text"] |= "connection error" in normalized
    return flags


def canonical_certificate_result(events):
    """Exact-message investigation inventory, not a production classifier."""
    inventory = {
        # Complete static message observed in the local packaged payload.
        # Still an investigation hint; no prefix matching or production evidence.
        "Unable to connect to API: Self-signed certificate detected. "
        "Check your proxy or corporate SSL certificates": "DEPTH_ZERO_SELF_SIGNED_CERT",
        "API Error: unable to verify the first certificate": "UNABLE_TO_VERIFY_LEAF_SIGNATURE",
        "API Error: unable to get local issuer certificate": "UNABLE_TO_GET_ISSUER_CERT_LOCALLY",
        "API Error: self signed certificate": "DEPTH_ZERO_SELF_SIGNED_CERT",
        "API Error: self-signed certificate": "DEPTH_ZERO_SELF_SIGNED_CERT",
        "API Error: self signed certificate in certificate chain": "SELF_SIGNED_CERT_IN_CHAIN",
        "API Error: self-signed certificate in certificate chain": "SELF_SIGNED_CERT_IN_CHAIN",
        "API Error: certificate has expired": "CERT_HAS_EXPIRED",
        "API Error: certificate verification failed": "CERTIFICATE_VERIFY_FAILED",
    }
    matches = set()
    for event in events:
        if (isinstance(event, dict) and event.get("type") == "result" and
                event.get("is_error") is True and isinstance(event.get("result"), str)):
            code = inventory.get(event["result"])
            if code:
                matches.add(code)
    return next(iter(matches)) if len(matches) == 1 else "unmatched"


def verify_payload(payload, metadata):
    if (metadata.get('platform') != 'windows' or metadata.get('architecture') != 'x64'
            or metadata.get('engineSize') != len(payload)
            or hashlib.sha256(payload).hexdigest() != metadata.get('engineSha256')):
        raise ValueError('Probe payload integrity mismatch')


def probe_environment(inherited, root):
    env = {key: value for key, value in inherited.items()
           if key.upper() in ('SYSTEMROOT', 'WINDIR', 'PATH')}
    home = root / 'home'
    env.update(HOME=str(home), USERPROFILE=str(home), CLAUDE_CONFIG_DIR=str(home),
        APPDATA=str(root / 'roaming'), LOCALAPPDATA=str(root / 'local'),
        TEMP=str(root / 'temp'), TMP=str(root / 'temp'),
        ANTHROPIC_AUTH_TOKEN='fixture-native-tls-dummy-token',
        CLAUDE_CODE_MAX_RETRIES='0', CLAUDE_CODE_RETRY_WATCHDOG='0',
        CLAUDE_CODE_DISABLE_NONSTREAMING_FALLBACK='1',
        CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC='1',
        DISABLE_AUTOUPDATER='1', DISABLE_TELEMETRY='1')
    return env


def load_fixture(name, filename):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def wait_process_handles(handles, wait, *, timeout=5):
    """Require process objects to be signaled within one shared cleanup budget."""
    deadline = time.monotonic() + timeout
    for handle in handles:
        milliseconds = max(0, int((deadline - time.monotonic()) * 1000))
        if wait(handle, milliseconds) != 0:
            raise RuntimeError('Probe process termination wait failed')


def run_contained(args, *, cwd, env, input, timeout):
    """Start suspended, contain before execution, and reap the entire Windows tree."""
    if sys.platform != 'win32':
        raise RuntimeError('Windows process containment required')
    import ctypes as c
    from ctypes import wintypes as w

    class BasicLimits(c.Structure):
        _fields_ = [('process_time', c.c_longlong), ('job_time', c.c_longlong),
                    ('flags', w.DWORD), ('minimum', c.c_size_t), ('maximum', c.c_size_t),
                    ('active_limit', w.DWORD), ('affinity', c.c_size_t),
                    ('priority', w.DWORD), ('scheduling', w.DWORD)]

    class ExtendedLimits(c.Structure):
        _fields_ = [('basic', BasicLimits), ('io', c.c_ulonglong * 6),
                    ('process_memory', c.c_size_t), ('job_memory', c.c_size_t),
                    ('peak_process_memory', c.c_size_t), ('peak_job_memory', c.c_size_t)]

    class Accounting(c.Structure):
        _fields_ = [('times', c.c_longlong * 4), ('faults', w.DWORD),
                    ('total', w.DWORD), ('active', w.DWORD), ('terminated', w.DWORD)]

    class ThreadEntry(c.Structure):
        _fields_ = [('size', w.DWORD), ('usage', w.DWORD), ('id', w.DWORD),
                    ('owner', w.DWORD), ('priority', w.LONG), ('delta', w.LONG),
                    ('flags', w.DWORD)]

    kernel = c.WinDLL('kernel32', use_last_error=True)
    signatures = {
        'CreateJobObjectW': ([c.c_void_p, w.LPCWSTR], w.HANDLE),
        'SetInformationJobObject': ([w.HANDLE, c.c_int, c.c_void_p, w.DWORD], w.BOOL),
        'AssignProcessToJobObject': ([w.HANDLE, w.HANDLE], w.BOOL),
        'TerminateJobObject': ([w.HANDLE, w.UINT], w.BOOL),
        'QueryInformationJobObject': ([w.HANDLE, c.c_int, c.c_void_p, w.DWORD, c.c_void_p], w.BOOL),
        'CreateToolhelp32Snapshot': ([w.DWORD, w.DWORD], w.HANDLE),
        'Thread32First': ([w.HANDLE, c.POINTER(ThreadEntry)], w.BOOL),
        'Thread32Next': ([w.HANDLE, c.POINTER(ThreadEntry)], w.BOOL),
        'OpenThread': ([w.DWORD, w.BOOL, w.DWORD], w.HANDLE),
        'ResumeThread': ([w.HANDLE], w.DWORD),
        'OpenProcess': ([w.DWORD, w.BOOL, w.DWORD], w.HANDLE),
        'WaitForSingleObject': ([w.HANDLE, w.DWORD], w.DWORD),
        'IsProcessInJob': ([w.HANDLE, w.HANDLE, c.POINTER(w.BOOL)], w.BOOL),
        'CloseHandle': ([w.HANDLE], w.BOOL),
    }
    for name, (arguments, result) in signatures.items():
        function = getattr(kernel, name)
        function.argtypes, function.restype = arguments, result

    job = kernel.CreateJobObjectW(None, None)
    if not job:
        raise RuntimeError('Probe containment creation failed')
    cleaned = False

    def reap_tree():
        nonlocal cleaned
        # ActiveProcesses may become zero before asynchronous termination has
        # signaled every process object. Retain job-member handles before kill.
        capacity = 16
        while True:
            buffer = c.create_string_buffer(8 + capacity * c.sizeof(c.c_size_t))
            if kernel.QueryInformationJobObject(job, 3, buffer, len(buffer), None):
                count = c.cast(c.addressof(buffer) + 4, c.POINTER(w.DWORD)).contents.value
                assigned_count = c.cast(buffer, c.POINTER(w.DWORD)).contents.value
                if count == assigned_count and count <= capacity:
                    ids = list((c.c_size_t * count).from_address(c.addressof(buffer) + 8))
                    break
                if capacity >= 65536:
                    raise RuntimeError('Probe process-tree enumeration failed')
                capacity *= 2
                continue
            if c.get_last_error() != 234 or capacity >= 65536:
                raise RuntimeError('Probe process-tree enumeration failed')
            capacity *= 2
        handles = []
        try:
            for pid in ids:
                handle = kernel.OpenProcess(0x100000 | 0x1000, False, pid)
                if not handle:
                    if c.get_last_error() == 87:  # Process already ceased to exist.
                        continue
                    raise RuntimeError('Probe process termination handle failed')
                handles.append(handle)
                member = w.BOOL()
                if not kernel.IsProcessInJob(handle, job, c.byref(member)):
                    raise RuntimeError('Probe process membership verification failed')
                if not member.value:
                    # PID was reused after the enumerated member exited.
                    kernel.CloseHandle(handles.pop())
            if not kernel.TerminateJobObject(job, 1):
                raise RuntimeError('Probe process-tree termination failed')
            wait_process_handles(handles, kernel.WaitForSingleObject)
            deadline = time.monotonic() + 5
            accounting = Accounting()
            while True:
                if not kernel.QueryInformationJobObject(job, 1, c.byref(accounting),
                                                       c.sizeof(accounting), None):
                    raise RuntimeError('Probe process-tree verification failed')
                if accounting.active == 0:
                    break
                if time.monotonic() >= deadline:
                    raise RuntimeError('Probe process-tree cleanup timed out')
                time.sleep(0.01)
            process.wait(timeout=5)
            cleaned = True
        finally:
            for handle in handles:
                kernel.CloseHandle(handle)

    process = None
    assigned = False
    try:
        limits = ExtendedLimits()
        limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if not kernel.SetInformationJobObject(job, 9, c.byref(limits), c.sizeof(limits)):
            raise RuntimeError('Probe containment configuration failed')
        # Files rather than inherited pipe readers keep orphan descendants from
        # blocking communicate() after the parent has already exited.
        with tempfile.TemporaryFile() as source, tempfile.TemporaryFile() as output, \
                tempfile.TemporaryFile() as errors:
            source.write(input.encode('utf-8'))
            source.seek(0)
            process = subprocess.Popen(args, cwd=cwd, env=env, stdin=source,
                stdout=output, stderr=errors, creationflags=0x4)  # CREATE_SUSPENDED
            if not kernel.AssignProcessToJobObject(job, w.HANDLE(int(process._handle))):
                raise RuntimeError('Probe containment assignment failed')
            assigned = True
            snapshot = kernel.CreateToolhelp32Snapshot(0x4, 0)  # TH32CS_SNAPTHREAD
            if snapshot == c.c_void_p(-1).value:
                raise RuntimeError('Probe suspended thread lookup failed')
            try:
                entry = ThreadEntry()
                entry.size = c.sizeof(entry)
                found = kernel.Thread32First(snapshot, c.byref(entry))
                resumed = False
                while found:
                    if entry.owner == process.pid:
                        thread = kernel.OpenThread(0x2, False, entry.id)
                        if not thread:
                            raise RuntimeError('Probe suspended thread open failed')
                        try:
                            if kernel.ResumeThread(thread) != 1:
                                raise RuntimeError('Probe suspended thread resume failed')
                            resumed = True
                        finally:
                            kernel.CloseHandle(thread)
                        break
                    found = kernel.Thread32Next(snapshot, c.byref(entry))
                if not resumed:
                    raise RuntimeError('Probe suspended thread missing')
            finally:
                kernel.CloseHandle(snapshot)
            try:
                process.wait(timeout=timeout)
            finally:
                # Reap on normal parent exit as well as timeout; native engines
                # may leave workers holding cwd and temporary storage open.
                reap_tree()
            output.seek(0)
            errors.seek(0)
            return subprocess.CompletedProcess(args, process.returncode,
                output.read().decode('utf-8'), errors.read().decode('utf-8'))
    finally:
        # If containment setup failed, the child has never been resumed. If a
        # subsequent operation failed, kill-on-close still covers descendants.
        try:
            if process is not None:
                if assigned:
                    if not cleaned:
                        reap_tree()
                else:
                    process.kill()
                    process.wait(timeout=5)
        finally:
            kernel.CloseHandle(job)


def probe(executable, *, tls12_only=False):
    if sys.platform != 'win32':
        raise RuntimeError('Windows native diagnostic probe required')
    integrity = load_fixture('probe_integrity', 'payload-integrity.py')
    gateway = load_fixture('probe_gateway', 'gateway-integration.py')
    payload, metadata_bytes = integrity.resources(executable)
    metadata = json.loads(metadata_bytes.decode('utf-8-sig'))
    verify_payload(payload, metadata)
    with tempfile.TemporaryDirectory(prefix='ccode-native-tls-probe-') as directory:
        root = Path(directory)
        engine = root / 'engine.exe'
        engine.write_bytes(payload)
        home = root / 'home'
        home.mkdir()
        workspace = root / 'workspace'
        workspace.mkdir()
        for storage in ('roaming', 'local', 'temp'):
            (root / storage).mkdir()
        env = probe_environment(os.environ, root)
        with gateway.untrusted_tls_endpoint(
                maximum_version=ssl.TLSVersion.TLSv1_2 if tls12_only else None) as endpoint:
            env['ANTHROPIC_BASE_URL'] = f'https://127.0.0.1:{endpoint.server_port}'
            try:
                result = run_contained([str(engine), '--print', '--output-format', 'stream-json',
                    '--verbose', '--tools', '', '--max-turns', '1', '--no-session-persistence'],
                    input='fixture-native-tls-probe', cwd=workspace, env=env, timeout=30)
            except subprocess.TimeoutExpired:
                print(json.dumps({'native_tls_probe': 'timeout', 'tls12_only': tls12_only}))
                return
            events = []
            invalid_lines = 0
            for line in result.stdout.splitlines():
                try:
                    events.append(json.loads(line))
                except json.JSONDecodeError:
                    invalid_lines += 1
            print(json.dumps({'native_tls_probe': summarize_events(events), 'tls12_only': tls12_only,
                'result_shape': summarize_result_shape(events),
                'failure_text_hints_not_tls_evidence': summarize_failure_text(events),
                'canonical_certificate_message_hint_not_tls_evidence': canonical_certificate_result(events),
                'engine_version': metadata['engineVersion'], 'exit_code': result.returncode,
                'connections': endpoint.connections,
                'tls_handshakes_completed': endpoint.tls_handshakes_completed,
                'tls_versions': endpoint.tls_versions, 'http_requests': endpoint.http_requests,
                'invalid_json_lines': invalid_lines, 'stderr_present': bool(result.stderr)}))


if __name__ == '__main__':
    probe(Path(sys.argv[1]).resolve())
    probe(Path(sys.argv[1]).resolve(), tls12_only=True)
