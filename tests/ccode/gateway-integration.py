"""Actual engine gateway rejection; local HTTP fixture and dummy credentials only."""
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def classify_tls_prefix(prefix):
    """Coarse record-header observation only; never return peer bytes."""
    if not prefix:
        return "closed"
    if len(prefix) >= 3 and prefix[0] == 22 and prefix[1] == 3 and prefix[2] <= 4:
        return "tls-record"
    return "other"


@contextmanager
def untrusted_tls_endpoint():
    """Loopback-only TLS fixture; the checked-in key is public test data, never trusted by the engine."""
    fixtures = Path(__file__).parent / "fixtures"
    certificate = fixtures / "untrusted-test-cert.pem"
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, fixtures / "untrusted-test-key.pem")

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            self.server.http_requests += 1
            self.send_response(503)
            self.send_header("Content-Length", "0")
            self.end_headers()

        do_POST = do_GET

    class Server(ThreadingHTTPServer):
        def get_request(self):
            connection, address = super().get_request()
            self.connections += 1
            connection.settimeout(2)
            try:
                # Peek without consuming bytes or retaining private peer content.
                self.protocol_observations.append(
                    classify_tls_prefix(connection.recv(3, socket.MSG_PEEK)))
                secure = context.wrap_socket(connection, server_side=True)
                self.tls_handshakes_completed += 1
                return secure, address
            except ssl.SSLError as error:
                self.handshake_errors.append({"kind": "tls", "errno": error.errno})
                self.handshake_failed.set()
                self.rejected.set()
                connection.close()
                raise
            except OSError as error:
                self.handshake_errors.append({"kind": "transport", "errno": error.errno})
                self.handshake_failed.set()
                connection.close()
                raise

    server = Server(("127.0.0.1", 0), Handler)
    server.address = server.server_address
    server.certificate = certificate
    server.rejected = threading.Event()
    server.handshake_failed = threading.Event()
    server.connections = 0
    server.tls_handshakes_completed = 0
    server.handshake_errors = []
    server.protocol_observations = []
    server.http_requests = 0
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=5)


@contextmanager
def unreachable_endpoint():
    # Keep ownership until the engine exits: closing an ephemeral listener would
    # allow another process to take the port and invalidate the refusal fixture.
    with socket.socket() as endpoint:
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            endpoint.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        endpoint.bind(("127.0.0.1", 0))
        yield endpoint.getsockname()


def check_unreachable(executable):
    with tempfile.TemporaryDirectory(prefix="ccode-unreachable-") as folder:
        root = Path(folder).resolve()
        program, workspace, data = root / "program", root / "workspace", root / "data"
        program.mkdir()
        workspace.mkdir()
        app = program / "ccode.exe"
        shutil.copy2(executable, app)
        token = "fixture-unreachable-dummy-token-81936"
        prompt = "fixture-private-unreachable-prompt-12974"
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(("A_", "C_", "ANTHROPIC_", "CLAUDE_", "CCODE_"))}
        report_path = root / "failure.json"
        with unreachable_endpoint() as address:
            # Verify refusal on the actual runner, not only in local unit tests.
            with socket.socket() as client:
                client.settimeout(2)
                assert client.connect_ex(address) != 0, "Unreachable fixture accepted a connection"
            env.update(A_AUTH_TOKEN=token, A_BASE_URL=f"http://{address[0]}:{address[1]}")
            try:
                result = subprocess.run([str(app), "--data-dir", str(data), "--print",
                    "--diagnostics", str(report_path), "--tools", "", prompt], cwd=workspace, env=env, input="",
                    capture_output=True, text=True, encoding="utf-8", timeout=60)
            except subprocess.TimeoutExpired:
                raise AssertionError("Unreachable gateway did not terminate within 60 seconds") from None
        terminal = result.stdout + result.stderr
        assert result.returncode != 0, "Unreachable gateway turn incorrectly succeeded"
        assert "E_" in terminal, "Unreachable gateway needs a neutral error"
        assert token not in terminal and prompt not in terminal, "Private fixture data leaked"
        report_code, category = stream_failure_diagnostic(terminal)
        validate_failure_diagnostic(report_path, report_code, result.returncode, category=category)
        assert not list(workspace.iterdir()), "Unreachable gateway changed workspace"
        assert not list(program.rglob("*.jsonl")), "History leaked into program directory"
        print("PASS: actual engine unreachable gateway terminates with neutral error, no workspace writes or terminal secret disclosure")


def tls_terminal_evidence(terminal):
    """Expose only booleans for allowlisted product-rendered diagnostics."""
    lines = set(terminal.splitlines())
    return {
        "tls": bool(lines & {"E_GATEWAY_TLS",
            "[E_GATEWAY_TLS: certificate verification failed]"}),
        "retry": "[E_GATEWAY_RETRY: automatic retry refused]" in lines,
        "engine": bool(lines & {"[E_ENGINE: request failed]",
            "[E_ENGINE: turn failed]",
            "[E_ENGINE: incomplete turn; check gateway and configuration]"}),
    }


def tls_rejection_observed(endpoint, returncode, diagnostic, *, handshake_timeout=0):
    """A transport abort alone is not evidence of engine certificate rejection."""
    return (endpoint.handshake_failed.wait(handshake_timeout) and endpoint.connections > 0
            and endpoint.http_requests == 0 and returncode != 0
            and diagnostic == "E_GATEWAY_TLS")


def check_tls_rejection(executable):
    with tempfile.TemporaryDirectory(prefix="ccode-tls-") as folder:
        root = Path(folder).resolve()
        program, workspace, data = root / "program", root / "workspace", root / "data"
        program.mkdir()
        workspace.mkdir()
        app = program / "ccode.exe"
        shutil.copy2(executable, app)
        token = "fixture-tls-dummy-token-31827"
        prompt = "fixture-private-tls-prompt-63418"
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(("A_", "C_", "ANTHROPIC_", "CLAUDE_", "CCODE_"))
               and key not in ("NODE_TLS_REJECT_UNAUTHORIZED", "NODE_EXTRA_CA_CERTS",
                               "SSL_CERT_FILE", "SSL_CERT_DIR")}
        report_path = root / "failure.json"
        with untrusted_tls_endpoint() as endpoint:
            env.update(A_AUTH_TOKEN=token,
                       A_BASE_URL=f"https://127.0.0.1:{endpoint.server_port}")
            try:
                result = subprocess.run([str(app), "--data-dir", str(data), "--print",
                    "--diagnostics", str(report_path), "--tools", "", prompt], cwd=workspace, env=env, input="",
                    capture_output=True, text=True, encoding="utf-8", timeout=60)
            except subprocess.TimeoutExpired:
                raise AssertionError("Untrusted TLS gateway did not terminate within 60 seconds") from None
            assert tls_rejection_observed(endpoint, result.returncode,
                "E_GATEWAY_TLS" if tls_terminal_evidence(result.stdout + result.stderr)["tls"] else None,
                handshake_timeout=3), (
                "No failed TLS handshake observed from actual engine; " + json.dumps({
                    "connections": endpoint.connections,
                    "handshake_errors": endpoint.handshake_errors,
                    "tls_handshakes_completed": endpoint.tls_handshakes_completed,
                    "protocol_observations": endpoint.protocol_observations,
                    "http_requests": endpoint.http_requests,
                    "exit_code": result.returncode,
                    "neutral_diagnostics": tls_terminal_evidence(result.stdout + result.stderr),
                }))
            assert endpoint.http_requests == 0, "Engine bypassed TLS trust and sent an HTTP request"
        terminal = result.stdout + result.stderr
        assert result.returncode != 0, "Untrusted TLS gateway turn incorrectly succeeded"
        assert token not in terminal and prompt not in terminal, "Private TLS fixture data leaked"
        validate_failure_diagnostic(report_path, "E_GATEWAY_TLS", result.returncode)
        assert not list(workspace.iterdir()), "Failed TLS changed workspace"
        assert not list(program.rglob("*.jsonl")), "History leaked into program directory"
        classifications = {code: code in terminal for code in
                           ("E_GATEWAY_TLS", "E_GATEWAY_RETRY", "E_ENGINE_API", "E_ENGINE_RESULT")}
        assert classifications["E_GATEWAY_TLS"], (
            "TLS failure needs its distinct neutral diagnostic: " + json.dumps(classifications))
        print("PASS: actual engine rejects untrusted TLS before HTTP, with TLS classification and no workspace writes or terminal secret disclosure")


def unfinished_tool_events(model, target, marker, complete_arguments=False):
    arguments = json.dumps({"file_path": target, "content": marker})
    if not complete_arguments:
        arguments = arguments[:-2]
    return [
        ("message_start", {"message": {
            "id": "msg_cut", "type": "message", "role": "assistant",
            "model": model, "content": [], "stop_reason": None,
            "usage": {"input_tokens": 10, "output_tokens": 0}}}),
        ("content_block_start", {"index": 0, "content_block": {
            "type": "tool_use", "id": "tool_cut", "name": "Write", "input": {}}}),
        ("content_block_delta", {"index": 0, "delta": {
            "type": "input_json_delta", "partial_json": arguments}}),
    ]


def validate_failure_diagnostic(path, expected_code, exit_code, *, category="network"):
    """A19: accept exactly the neutral schema, not arbitrary extra private fields."""
    try:
        report = json.loads(path.read_text(encoding='utf-8'))
        operation_id = report['operationId']
        identifier = uuid.UUID(operation_id)
    except (OSError, ValueError, TypeError, KeyError, AttributeError):
        raise AssertionError('Missing or invalid neutral gateway failure report') from None
    assert identifier.version == 4 and str(identifier) == operation_id.lower(), 'Invalid operation ID'
    expected = {'schemaVersion': 1, 'product': 'ccode', 'platform': 'windows',
                'architecture': 'x64', 'status': 'error', 'operationId': operation_id,
                'errorCode': expected_code, 'category': category, 'exitCode': exit_code,
                'privacy': dict.fromkeys(('argumentsCaptured', 'environmentValuesCaptured',
                                         'promptOrContentCaptured', 'credentialsCaptured'), False)}
    assert json.dumps(report, sort_keys=True) == json.dumps(expected, sort_keys=True), (
        'Gateway failure report lost cause or violates neutral schema')


def stream_failure_diagnostic(terminal):
    """Choose only exact neutral renderer lines, never substring-match private text."""
    lines = set(terminal.splitlines())
    candidates = (
        ('E_GATEWAY_RETRY', 'network', '[E_GATEWAY_RETRY: automatic retry refused]'),
        ('E_MISSING_RESULT', 'protocol', '[E_MISSING_RESULT: incomplete turn]'),
        ('E_TRUNCATED_EVENT', 'protocol', '[E_TRUNCATED_EVENT: incomplete turn]'),
        ('E_PROTOCOL_JSON', 'protocol', '[E_PROTOCOL_JSON: invalid engine event]'),
        ('E_PROTOCOL_SCHEMA', 'protocol', '[E_PROTOCOL_SCHEMA: invalid engine event]'),
        ('E_ENGINE', 'local', '[E_ENGINE: turn failed]'),
        ('E_ENGINE', 'local', '[E_ENGINE: request failed]'),
        ('E_ENGINE', 'local', '[E_ENGINE: incomplete turn; check gateway and configuration]'),
    )
    for code, category, rendered in candidates:
        if rendered in lines:
            return code, category
    raise AssertionError('Interrupted stream lacks a known neutral failure classification')


def check_rejection(executable, status_code, error_type, diagnostic, stream_cut=False,
                    graceful_eof=False, complete_arguments=False):
    with tempfile.TemporaryDirectory(prefix="ccode-gateway-") as folder:
        root = Path(folder).resolve()
        program = root / "program"
        workspace = root / "workspace"
        data = root / "data"
        program.mkdir()
        workspace.mkdir()
        app = program / "ccode.exe"
        shutil.copy2(executable, app)
        requests = []
        cut_delivered = threading.Event()
        private_marker = "fixture-private-gateway-detail-87319"
        token = "fixture-dummy-token-48219"

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
                path = self.path.split("?")[0]
                requests.append((path, body))
                if stream_cut and path == "/v1/messages":
                    request = json.loads(body)
                    assert request.get("stream"), "Fixture requires a streaming request"
                    events = unfinished_tool_events(request["model"],
                        str(workspace / "must-not-exist.txt"), private_marker, complete_arguments)
                    payload = "".join(f"event: {kind}\ndata: {json.dumps(dict(value, type=kind))}\n\n"
                                      for kind, value in events).encode()
                    self.send_response(200)
                    self.send_header("Content-Type", "text/event-stream")
                    # Also exercise clean HTTP EOF with missing semantic end events:
                    # a valid transport must not make partial tool arguments executable.
                    self.send_header("Content-Length", str(len(payload) + (0 if graceful_eof else 100)))
                    self.end_headers()
                    self.wfile.write(payload)
                    self.wfile.flush()
                    self.close_connection = True
                    cut_delivered.set()
                    return
                if path == "/v1/messages/count_tokens":
                    status, response = 200, {"input_tokens": 100}
                else:
                    status, response = status_code, {"type": "error", "error": {
                        "type": error_type, "message": private_marker}}
                payload = json.dumps(response).encode()
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(payload)))
                if status == 429:
                    self.send_header("Retry-After", "1")
                self.end_headers()
                self.wfile.write(payload)

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(("A_", "C_", "ANTHROPIC_", "CLAUDE_", "CCODE_"))}
        env.update(A_AUTH_TOKEN=token, A_BASE_URL=f"http://127.0.0.1:{server.server_port}")
        try:
            tool_options = ["--tools", "Write", "--allowedTools", "Write"] if stream_cut else ["--tools", ""]
            report_path = root / "failure.json"
            report_options = ["--diagnostics", str(report_path)]
            try:
                result = subprocess.run([str(app), "--data-dir", str(data), "--print",
                                     *report_options, *tool_options, "gateway rejection fixture"],
                                    cwd=workspace, env=env, input="", capture_output=True,
                                    text=True, encoding="utf-8", timeout=60)
            except subprocess.TimeoutExpired as error:
                # Report only counts and booleans: never dump gateway bodies,
                # prompts, credentials, or unfiltered engine output into CI.
                stdout = error.stdout or b""
                stderr = error.stderr or b""
                if isinstance(stdout, str):
                    stdout = stdout.encode("utf-8")
                if isinstance(stderr, str):
                    stderr = stderr.encode("utf-8")
                terminal = stdout + stderr
                diagnostics = {
                    "message_requests": sum(path == "/v1/messages" for path, _ in requests),
                    "token_count_requests": sum(path == "/v1/messages/count_tokens" for path, _ in requests),
                    "other_requests": sum(path not in ("/v1/messages", "/v1/messages/count_tokens")
                                          for path, _ in requests),
                    "stdout_bytes": len(stdout), "stderr_bytes": len(stderr),
                    "private_marker_visible": private_marker.encode() in terminal,
                    "credential_visible": token.encode() in terminal,
                    "neutral_error_visible": b"E_" in terminal,
                }
                raise AssertionError("Gateway rejection timed out: " + json.dumps(diagnostics)) from None
            assert result.returncode != 0, "Failed gateway turn incorrectly succeeded"
            if stream_cut:
                assert cut_delivered.is_set(), "Truncated tool arguments were not delivered"
            messages = [body for path, body in requests if path == "/v1/messages"]
            if len(messages) != 1:
                summaries = []
                for body in messages:
                    request = json.loads(body)
                    history = request.get("messages", [])
                    blocks = [block for message in history
                              if isinstance(message.get("content"), list)
                              for block in message["content"] if isinstance(block, dict)]
                    summaries.append({
                        "same_body_as_first": body == messages[0],
                        "stream": request.get("stream") is True,
                        "history_messages": len(history),
                        "tool_uses": sum(block.get("type") == "tool_use" for block in blocks),
                        "tool_results": sum(block.get("type") == "tool_result" for block in blocks),
                        "tool_errors": sum(block.get("type") == "tool_result" and
                                           block.get("is_error") is True for block in blocks),
                    })
                raise AssertionError("Model request replay: " + json.dumps({
                    "requests": summaries, "workspace_changed": bool(list(workspace.iterdir()))}))
            terminal = result.stdout + result.stderr
            assert private_marker not in terminal and token not in terminal, "Gateway details leaked to terminal"
            assert diagnostic in terminal, f"HTTP {status_code} needs its neutral diagnostic"
            if stream_cut:
                report_code, category = stream_failure_diagnostic(terminal)
                validate_failure_diagnostic(report_path, report_code, result.returncode, category=category)
            else:
                validate_failure_diagnostic(report_path, diagnostic, result.returncode)
            assert not list(workspace.iterdir()), "Rejected request changed workspace"
            assert not list(program.rglob("*.jsonl")), "History leaked into program directory"
            scenario = ("complete tool JSON without block termination at clean HTTP EOF" if complete_arguments
                        else "unfinished tool stream at clean HTTP EOF" if graceful_eof
                        else "truncated tool stream" if stream_cut else f"HTTP {status_code}")
            print(f"PASS: actual engine {scenario} fails without model-request replay, workspace writes or terminal secret disclosure")
        finally:
            server.shutdown()
            server.server_close()
            worker.join(timeout=5)


if __name__ == "__main__":
    executable = Path(sys.argv[1]).resolve()
    check_unreachable(executable)
    check_rejection(executable, 401, "authentication_error", "E_GATEWAY_AUTH")
    check_rejection(executable, 429, "rate_limit_error", "E_GATEWAY_RATE_LIMIT")
    check_rejection(executable, 200, None, "E_", stream_cut=True)
    check_rejection(executable, 200, None, "E_", stream_cut=True, graceful_eof=True)
    check_rejection(executable, 200, None, "E_", stream_cut=True, graceful_eof=True, complete_arguments=True)
    check_tls_rejection(executable)
