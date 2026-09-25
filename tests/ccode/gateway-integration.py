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


def check_unauthorized(executable):
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
        private_marker = "fixture-private-gateway-detail-87319"
        token = "fixture-dummy-token-48219"

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
                path = self.path.split("?")[0]
                requests.append((path, body))
                if path == "/v1/messages/count_tokens":
                    status, response = 200, {"input_tokens": 100}
                else:
                    status, response = 401, {"type": "error", "error": {
                        "type": "authentication_error", "message": private_marker}}
                payload = json.dumps(response).encode()
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(("A_", "C_", "ANTHROPIC_", "CLAUDE_", "CCODE_"))}
        env.update(A_AUTH_TOKEN=token, A_BASE_URL=f"http://127.0.0.1:{server.server_port}")
        try:
            result = subprocess.run([str(app), "--data-dir", str(data), "--print",
                                     "--tools", "", "gateway rejection fixture"],
                                    cwd=workspace, env=env, input="", capture_output=True,
                                    text=True, encoding="utf-8", timeout=60)
            assert result.returncode != 0, (result.returncode, result.stdout, result.stderr)
            messages = [body for path, body in requests if path == "/v1/messages"]
            assert len(messages) == 1, "401 must not replay the model request"
            terminal = result.stdout + result.stderr
            assert private_marker not in terminal and token not in terminal, "Gateway details leaked to terminal"
            assert "E_" in terminal, "Failure needs a neutral diagnostic"
            assert not list(workspace.iterdir()), "Rejected request changed workspace"
            assert not list(program.rglob("*.jsonl")), "History leaked into program directory"
            print("PASS: actual engine HTTP 401 fails without model-request replay, workspace writes or terminal secret disclosure")
        finally:
            server.shutdown()
            server.server_close()
            worker.join(timeout=5)


if __name__ == "__main__":
    check_unauthorized(Path(sys.argv[1]).resolve())
