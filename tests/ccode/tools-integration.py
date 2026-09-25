"""Real Windows engine tools, driven by a deterministic local API (no live model).

The API response is a fixture; tool execution, files, child processes and frontend
are real. This does NOT establish live-model quality or third-party compatibility.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def check(executable):
    with tempfile.TemporaryDirectory(prefix="ccode-tools-") as temporary:
        root = Path(temporary)
        app_dir = root / "portable app 中文"
        workspace = root / "workspace 中文 with spaces"
        data = root / "persistent data"
        app_dir.mkdir()
        workspace.mkdir()
        app = app_dir / "ccode.exe"
        shutil.copy2(executable, app)
        target = workspace / "claude-anthropic-original.txt"
        plan = [
            ("Write", {"file_path": str(target), "content": "marker-before\n"}),
            ("Edit", {"file_path": str(target), "old_string": "marker-before", "new_string": "marker-after"}),
            ("Read", {"file_path": str(target)}),
            ("Grep", {"pattern": "marker-after", "path": str(workspace), "output_mode": "content"}),
            ("Glob", {"pattern": "*.txt", "path": str(workspace)}),
            ("Bash", {"command": "mkdir -p 'runtime tasks' && printf 'shell-marker' > 'runtime tasks/probe.txt' && cat 'runtime tasks/probe.txt'", "description": "Exercise workspace filesystem"}),
        ]
        received = {}
        requests = []
        handler_errors = []

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                try:
                    self.respond()
                except Exception as error:
                    handler_errors.append(repr(error))
                    self.send_error(500)

            def respond(self):
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
                for message in body.get("messages", []):
                    content = message.get("content", [])
                    if not isinstance(content, list):
                        continue
                    for block in content:
                        if block.get("type") == "tool_result":
                            received[block["tool_use_id"]] = block
                step = next((i for i in range(len(plan)) if f"acceptance_{i}" not in received), len(plan))
                if step < len(plan):
                    name, arguments = plan[step]
                    block = dict(type="tool_use", id=f"acceptance_{step}", name=name, input=arguments)
                    reason = "tool_use"
                else:
                    block = dict(type="text", text="tools-acceptance-complete")
                    reason = "end_turn"
                message = dict(id=f"msg_acceptance_{step}", type="message", role="assistant",
                               model=body.get("model", "fixture"), content=[block], stop_reason=reason,
                               stop_sequence=None, usage=dict(input_tokens=100, output_tokens=20))
                if body.get("stream"):
                    events = [("message_start", {"message": dict(message, content=[], stop_reason=None)})]
                    if block["type"] == "tool_use":
                        events.append(("content_block_start", {"index": 0, "content_block": dict(block, input={})}))
                        encoded = json.dumps(block["input"], ensure_ascii=False)
                        # Final arguments are valid; intermediate fragments intentionally are not.
                        for offset in range(0, len(encoded), 7):
                            events.append(("content_block_delta", {"index": 0, "delta": {
                                "type": "input_json_delta", "partial_json": encoded[offset:offset + 7]}}))
                    else:
                        events.extend([
                            ("content_block_start", {"index": 0, "content_block": {"type": "text", "text": ""}}),
                            ("content_block_delta", {"index": 0, "delta": {"type": "text_delta", "text": block["text"]}}),
                        ])
                    events.extend([
                        ("content_block_stop", {"index": 0}),
                        ("message_delta", {"delta": {"stop_reason": reason, "stop_sequence": None}, "usage": {"output_tokens": 20}}),
                        ("message_stop", {}),
                    ])
                    payload = "".join(f"event: {kind}\ndata: {json.dumps(dict(value, type=kind))}\n\n"
                                      for kind, value in events).encode()
                    content_type = "text/event-stream"
                else:
                    payload = json.dumps(message).encode()
                    content_type = "application/json"
                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(("A_", "C_", "ANTHROPIC_", "CLAUDE_", "CCODE_"))}
        env.update(A_AUTH_TOKEN="acceptance-test-only", A_BASE_URL=f"http://127.0.0.1:{server.server_port}")
        try:
            result = subprocess.run([str(app), "--data-dir", str(data), "--print",
                                     "--allowedTools", "Write,Edit,Read,Grep,Glob,Bash",
                                     "Exercise the six tools in this workspace."],
                                    cwd=workspace, env=env, input="", capture_output=True,
                                    text=True, encoding="utf-8", errors="replace", timeout=120)
            assert not handler_errors, handler_errors
            assert result.returncode == 0, (result.returncode, result.stdout, result.stderr, received)
            assert "tools-acceptance-complete" in result.stdout, result.stdout
            assert len(received) == len(plan), received
            if any(block.get("is_error") for block in received.values()):
                print("Frontend tool results:", json.dumps(received, ensure_ascii=True), flush=True)
                # Differential diagnosis only: never a production fallback. Identical fake API,
                # workspace and profile environment, using the unmodified embedded engine.
                payload = next(app_dir.glob("runtime/*/engine.exe"))
                native_env = env.copy()
                profile = data / "profile"
                native_env.update(ANTHROPIC_AUTH_TOKEN=env["A_AUTH_TOKEN"],
                                  ANTHROPIC_BASE_URL=env["A_BASE_URL"],
                                  HOME=str(profile / "home"), USERPROFILE=str(profile / "home"),
                                  APPDATA=str(profile / "roaming"), LOCALAPPDATA=str(profile / "local"),
                                  TEMP=str(profile / "temp"), TMP=str(profile / "temp"),
                                  CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC="1", DISABLE_AUTOUPDATER="1")
                debug = root / "native-debug.txt"
                saved_results = received.copy()
                received.clear()
                native = subprocess.run([str(payload), "--print", "--output-format", "stream-json", "--verbose",
                                         "--allowedTools", "Write,Edit,Read,Grep,Glob,Bash", "--debug-file", str(debug)],
                                        input="Exercise fixture tools.", cwd=workspace, env=native_env,
                                        capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
                print("Native comparison exit:", native.returncode, flush=True)
                print("Native tool results:", json.dumps(received, ensure_ascii=True), flush=True)
                if debug.exists():
                    lines = [line for line in debug.read_text(encoding="utf-8", errors="replace").splitlines()
                             if "permission" in line.lower() or "allowedtools" in line.lower()]
                    print("Fixture-only native permission diagnostics:",
                          json.dumps(lines[-30:], ensure_ascii=True), flush=True)
                received.clear()
                received.update(saved_results)
            for i, (name, _) in enumerate(plan):
                assert not received[f"acceptance_{i}"].get("is_error"), (name, received[f"acceptance_{i}"])
            for i, marker in [(2, "marker-after"), (3, "marker-after"),
                              (4, target.name), (5, "shell-marker")]:
                assert marker in json.dumps(received[f"acceptance_{i}"]), received[f"acceptance_{i}"]
            assert target.read_text() == "marker-after\n"
            assert (workspace / "runtime tasks/probe.txt").read_text() == "shell-marker"
            assert not (app_dir / "data").exists(), "Explicit data directory was ignored"
            assert not list(app_dir.rglob("*.jsonl")), "Session leaked into program directory"
            assert list(data.rglob("*.jsonl")), "No authoritative session was saved"
            assert not list(app_dir.rglob("*.dll")), "Unexpected injected runtime"
            print("PASS: real Write/Edit/Read/Grep/Glob/Bash; Unicode/spaces; external data; fragmented arguments")
        finally:
            server.shutdown()
            server.server_close()


if __name__ == "__main__":
    check(Path(sys.argv[1]).resolve())
