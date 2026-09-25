"""Actual engine gateway rejection; local HTTP fixture and dummy credentials only."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def check_rejection(executable, status_code, error_type, diagnostic, stream_cut=False):
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
                    partial = json.dumps({"file_path": str(workspace / "must-not-exist.txt"),
                                          "content": private_marker})[:-2]
                    events = [
                        ("message_start", {"message": {
                            "id": "msg_cut", "type": "message", "role": "assistant",
                            "model": request["model"], "content": [], "stop_reason": None,
                            "usage": {"input_tokens": 10, "output_tokens": 0}}}),
                        ("content_block_start", {"index": 0, "content_block": {
                            "type": "tool_use", "id": "tool_cut", "name": "Write", "input": {}}}),
                        ("content_block_delta", {"index": 0, "delta": {
                            "type": "input_json_delta", "partial_json": partial}}),
                    ]
                    payload = "".join(f"event: {kind}\ndata: {json.dumps(dict(value, type=kind))}\n\n"
                                      for kind, value in events).encode()
                    self.send_response(200)
                    self.send_header("Content-Type", "text/event-stream")
                    # Promise more bytes, then close before the arguments or turn end.
                    self.send_header("Content-Length", str(len(payload) + 100))
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
            try:
                result = subprocess.run([str(app), "--data-dir", str(data), "--print",
                                     *tool_options, "gateway rejection fixture"],
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
            assert len(messages) == 1, f"HTTP {status_code} must not replay the model request"
            terminal = result.stdout + result.stderr
            assert private_marker not in terminal and token not in terminal, "Gateway details leaked to terminal"
            assert diagnostic in terminal, f"HTTP {status_code} needs its neutral diagnostic"
            assert not list(workspace.iterdir()), "Rejected request changed workspace"
            assert not list(program.rglob("*.jsonl")), "History leaked into program directory"
            scenario = "truncated tool stream" if stream_cut else f"HTTP {status_code}"
            print(f"PASS: actual engine {scenario} fails without model-request replay, workspace writes or terminal secret disclosure")
        finally:
            server.shutdown()
            server.server_close()
            worker.join(timeout=5)


if __name__ == "__main__":
    executable = Path(sys.argv[1]).resolve()
    check_rejection(executable, 401, "authentication_error", "E_GATEWAY_AUTH")
    check_rejection(executable, 429, "rate_limit_error", "E_GATEWAY_RATE_LIMIT")
    check_rejection(executable, 200, None, "E_", stream_cut=True)
