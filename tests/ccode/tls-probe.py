"""Privacy-safe structural diagnostics; not an acceptance substitute."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


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


def probe(executable):
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
        with gateway.untrusted_tls_endpoint() as endpoint:
            env['ANTHROPIC_BASE_URL'] = f'https://127.0.0.1:{endpoint.server_port}'
            try:
                result = subprocess.run([str(engine), '--print', '--output-format', 'stream-json',
                    '--verbose', '--tools', '', '--max-turns', '1', '--no-session-persistence'],
                    input='fixture-native-tls-probe', cwd=workspace, env=env, capture_output=True,
                    text=True, encoding='utf-8', timeout=30)
            except subprocess.TimeoutExpired:
                print(json.dumps({'native_tls_probe': 'timeout'}))
                return
            events = []
            invalid_lines = 0
            for line in result.stdout.splitlines():
                try:
                    events.append(json.loads(line))
                except json.JSONDecodeError:
                    invalid_lines += 1
            print(json.dumps({'native_tls_probe': summarize_events(events),
                'engine_version': metadata['engineVersion'], 'exit_code': result.returncode,
                'connections': endpoint.connections, 'http_requests': endpoint.http_requests,
                'invalid_json_lines': invalid_lines, 'stderr_present': bool(result.stderr)}))


if __name__ == '__main__':
    probe(Path(sys.argv[1]).resolve())
