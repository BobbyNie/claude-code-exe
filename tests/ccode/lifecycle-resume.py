"""Resume an existing external-data session after the complete program directory moves.

The loopback response is deterministic; this is not a live model or enterprise gateway.
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import sys
import threading


def response_payload(body):
    message = {
        "id": "msg_lifecycle_resume",
        "type": "message",
        "role": "assistant",
        "model": body.get("model", "fixture"),
        "content": [{"type": "text", "text": "lifecycle-resume-complete"}],
        "stop_reason": "end_turn",
        "stop_sequence": None,
        "usage": {"input_tokens": 100, "output_tokens": 10},
    }
    if not body.get("stream"):
        return "application/json", json.dumps(message).encode()
    events = [
        ("message_start", {"message": dict(message, content=[], stop_reason=None)}),
        ("content_block_start", {"index": 0, "content_block": {"type": "text", "text": ""}}),
        ("content_block_delta", {"index": 0, "delta": {"type": "text_delta", "text": "lifecycle-resume-complete"}}),
        ("content_block_stop", {"index": 0}),
        ("message_delta", {"delta": {"stop_reason": "end_turn", "stop_sequence": None},
                           "usage": {"output_tokens": 10}}),
        ("message_stop", {}),
    ]
    payload = "".join(f"event: {kind}\ndata: {json.dumps(dict(value, type=kind))}\n\n"
                      for kind, value in events).encode()
    return "text/event-stream", payload


def check(executable, workspace, data, expected_identity):
    requests = []
    errors = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            try:
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                if self.path.split("?")[0] == "/v1/messages/count_tokens":
                    self.send_response(200)
                    self.end_headers()
                    self.wfile.write(b'{"input_tokens":100}')
                    return
                if self.path.split("?")[0] != "/v1/messages":
                    self.send_error(404)
                    return
                requests.append(body)
                content_type, payload = response_payload(body)
                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
            except Exception as error:  # acceptance evidence records only this assertion
                errors.append(repr(error))
                self.send_error(500)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(("A_", "C_", "ANTHROPIC_", "CLAUDE_", "CCODE_"))}
        env.update(A_AUTH_TOKEN="acceptance-test-only",
                   A_BASE_URL=f"http://127.0.0.1:{server.server_port}")
        identity = subprocess.run([str(executable), "--data-dir", str(data), "--workspace-id"],
                                  cwd=workspace, env=env, capture_output=True, text=True,
                                  encoding="utf-8", errors="replace", timeout=30)
        assert identity.returncode == 0, identity.stderr
        assert identity.stdout.strip() == expected_identity, "Program relocation changed workspace identity"
        sessions = subprocess.run([str(executable), "--data-dir", str(data), "--sessions"],
                                  cwd=workspace, env=env, capture_output=True, text=True,
                                  encoding="utf-8", errors="replace", timeout=30)
        assert sessions.returncode == 0 and "No saved sessions" not in sessions.stdout, sessions.stderr
        original_prompt_marker = "Exercise the six tools in this workspace."
        resumed = subprocess.run([str(executable), "--data-dir", str(data), "--print", "--continue",
                                  "Continue after complete program directory relocation."],
                                 cwd=workspace, env=env, capture_output=True, text=True,
                                 encoding="utf-8", errors="replace", timeout=90)
        assert resumed.returncode == 0, resumed.stdout + resumed.stderr
        assert "lifecycle-resume-complete" in resumed.stdout, resumed.stdout
        assert not errors and requests, errors
        history = json.dumps(requests[-1].get("messages", []), ensure_ascii=False)
        assert original_prompt_marker in history, "Relocated program did not send original saved history"
        print(json.dumps({"schema": 1, "status": "passed", "workspaceIdentity": expected_identity,
                          "originalHistoryObserved": True,
                          "loopbackFixture": "not a live model or enterprise gateway"}, sort_keys=True))
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    if len(sys.argv) != 5:
        raise SystemExit("usage: lifecycle-resume.py EXECUTABLE WORKSPACE DATA EXPECTED_IDENTITY")
    check(*(Path(value).resolve() if index < 3 else value
            for index, value in enumerate(sys.argv[1:])))
