"""Exercise real --resume against a local fake API, without user credentials."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import threading
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def prepare(executable):
    root = executable.parent
    session = str(uuid.uuid4())
    project = re.sub(r"[^a-zA-Z0-9]", "-", str(root))
    legacy = root / "data/cc/profile/home/.cc/projects" / project
    legacy.mkdir(parents=True, exist_ok=True)
    user_id = str(uuid.uuid4())
    common = dict(sessionId=session, cwd=str(root), version="2.1.221",
                  isSidechain=False, userType="external",
                  timestamp="2026-09-25T00:00:00.000Z")
    messages = [
        dict(common, type="user", uuid=user_id, parentUuid=None,
             message={"role": "user", "content": "legacy-resume-marker-7391"}),
        dict(common, type="assistant", uuid=str(uuid.uuid4()), parentUuid=user_id,
             message={"id": "msg_fixture", "type": "message", "role": "assistant",
                      "model": "claude-sonnet-4-6", "stop_reason": "end_turn",
                      "stop_sequence": None,
                      "content": [{"type": "text", "text": "Recorded the marker."}],
                      "usage": {"input_tokens": 10, "output_tokens": 5}}),
    ]
    (legacy / f"{session}.jsonl").write_text(
        "".join(json.dumps(message) + "\n" for message in messages), encoding="utf-8")
    (root / "resume-fixture-id.txt").write_text(session, encoding="utf-8")


def verify(executable):
    requests = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            if "count_tokens" in self.path:
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(b'{"input_tokens":100}')
                return
            if self.path.split("?")[0] != "/v1/messages":
                self.send_error(404)
                return
            requests.append(body)
            message = {"id": "msg_resume_test", "type": "message", "role": "assistant",
                       "model": body.get("model", "claude-sonnet-4-6"),
                       "content": [{"type": "text", "text": "resume-test-ok"}],
                       "stop_reason": "end_turn", "stop_sequence": None,
                       "usage": {"input_tokens": 100, "output_tokens": 5}}
            if body.get("stream"):
                events = [
                    ("message_start", {"message": dict(message, content=[], stop_reason=None)}),
                    ("content_block_start", {"index": 0, "content_block": {"type": "text", "text": ""}}),
                    ("content_block_delta", {"index": 0, "delta": {"type": "text_delta", "text": "resume-test-ok"}}),
                    ("content_block_stop", {"index": 0}),
                    ("message_delta", {"delta": {"stop_reason": "end_turn", "stop_sequence": None},
                                       "usage": {"output_tokens": 5}}),
                    ("message_stop", {}),
                ]
                data = "".join(f"event: {kind}\ndata: {json.dumps(dict(value, type=kind))}\n\n"
                               for kind, value in events).encode()
                content_type = "text/event-stream"
            else:
                data = json.dumps(message).encode()
                content_type = "application/json"
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    env = os.environ.copy()
    env.update(A_AUTH_TOKEN="test-only-token", A_BASE_URL=f"http://127.0.0.1:{server.server_port}")
    session = (executable.parent / "resume-fixture-id.txt").read_text().strip()
    try:
        result = subprocess.run([str(executable), "--resume", session, "--print", "Reply with OK."],
                                cwd=executable.parent, env=env, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=60)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "resume-test-ok" in result.stdout, result.stdout + result.stderr
        assert any("legacy-resume-marker-7391" in json.dumps(request.get("messages", []))
                   for request in requests), "Official runtime did not load legacy session history"
        print("ccode official --resume integration test passed")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    executable = Path(sys.argv[2]).resolve()
    {"prepare": prepare, "verify": verify}[sys.argv[1]](executable)
