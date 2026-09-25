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
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def check_lifecycle(app, workspace, data, env, mode):
    """Observe a real grandchild, then cancel/crash the frontend and verify cleanup."""
    import ctypes
    import signal
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    kernel.TerminateProcess.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    allocated = not kernel.GetConsoleCP()
    if allocated:
        assert kernel.AllocConsole(), "Cannot create test console for Ctrl+Break"
    process = None
    child = None
    try:
        process = subprocess.Popen([str(app), "--data-dir", str(data), "--print",
                                    "--allowedTools", "Bash", "Run the process-tree fixture."],
                                   cwd=workspace, env=env, stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
        marker = workspace / "child.pid"
        deadline = time.monotonic() + 45
        while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.05)
        assert marker.exists(), ("Tool grandchild did not start", process.poll())
        pid = int(marker.read_text())
        child = kernel.OpenProcess(0x00100001, False, pid)  # SYNCHRONIZE | PROCESS_TERMINATE
        assert child and kernel.WaitForSingleObject(child, 0) == 258, "Grandchild is not alive"
        if mode == "cancel":
            process.send_signal(signal.CTRL_BREAK_EVENT)
        else:
            process.kill()  # crash: no frontend cleanup code can run
        output, errors = process.communicate(timeout=15)
        assert kernel.WaitForSingleObject(child, 5000) == 0, "Frontend left a running tool grandchild"
        if mode == "cancel":
            assert process.returncode == 130, (process.returncode, output, errors)
            assert b"Cancelled" in errors, errors
        else:
            assert process.returncode != 0
        assert not (workspace / "after-wait.txt").exists(), "Cancelled tool continued its side effects"
        print(f"PASS: {mode} terminates actual tool process tree without orphan grandchildren")
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.communicate(timeout=15)
        if child:
            if kernel.WaitForSingleObject(child, 0) == 258:
                kernel.TerminateProcess(child, 99)  # clean up the fixture even on a failing implementation
                kernel.WaitForSingleObject(child, 5000)
            kernel.CloseHandle(child)
        if allocated:
            kernel.FreeConsole()


def check(executable, short_path=False, lifecycle=None, permission=None, long_workspace=False):
    with tempfile.TemporaryDirectory(prefix="ccode-tools-") as temporary:
        root = Path(temporary).resolve()
        if short_path:
            import ctypes
            buffer = ctypes.create_unicode_buffer(32768)
            get_short = ctypes.windll.kernel32.GetShortPathNameW
            get_short.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint32]
            get_short.restype = ctypes.c_uint32
            count = get_short(str(root), buffer, len(buffer))
            assert 0 < count < len(buffer), "Unable to prepare 8.3 path case"
            root = Path(buffer.value)
            assert "~" in str(root), "Runner must support 8.3 names for this acceptance case"
        app_dir = root / "portable app 中文"
        workspace = root / "workspace 中文 with spaces"
        if long_workspace:
            workspace = workspace / ("long-segment-" * 10) / ("nested-segment-" * 8)
            assert len(str(workspace)) > 260
        data = root / "persistent data"
        app_dir.mkdir()
        workspace.mkdir(parents=True)
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
        if lifecycle:
            script = workspace / "wait-child.ps1"
            script.write_text("[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'child.pid'), [string]$PID)\n"
                              "Start-Sleep -Seconds 300\n"
                              "[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'after-wait.txt'), 'unexpected')\n")
            plan = [("Bash", {"command": "powershell.exe -NoProfile -File ./wait-child.ps1",
                              "description": "Wait for lifecycle acceptance signal", "timeout": 600000})]
        if permission:
            target = workspace / f"permission-{permission}-{time.time_ns()}.txt"
            plan = [("Write", {"file_path": str(target), "content": "approved-write\n"})]
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
        def recover_after_interruption():
            nonlocal plan
            # Reuse the exact profile, without deleting a lock or repairing files
            # in the test. A fresh turn must not mutate interrupted transcripts.
            transcripts = {path: path.read_bytes() for path in data.rglob("*.jsonl")}
            assert transcripts, "Interrupted run did not leave authoritative session evidence"
            history = subprocess.run([str(app), "--data-dir", str(data), "--sessions"],
                                     cwd=workspace, env=env, input="", capture_output=True,
                                     text=True, encoding="utf-8", timeout=15)
            assert history.returncode == 0, (history.returncode, history.stdout, history.stderr)
            assert "No saved sessions" not in history.stdout, history.stdout
            assert any(path.stem in history.stdout for path in transcripts), history.stdout
            marker = workspace / "recovered-write.txt"
            plan = [("Write", {"file_path": str(marker), "content": "recovered-turn\n"})]
            received.clear()
            recovered = subprocess.run([str(app), "--data-dir", str(data), "--print",
                                        "--allowedTools", "Write", "Perform the recovery write."],
                                       cwd=workspace, env=env, input="", capture_output=True,
                                       text=True, encoding="utf-8", timeout=60)
            assert recovered.returncode == 0, (recovered.returncode, recovered.stdout, recovered.stderr)
            assert not received["acceptance_0"].get("is_error"), received
            assert marker.read_text() == "recovered-turn\n"
            for path, content in transcripts.items():
                assert path.read_bytes() == content, "Recovery rewrote an interrupted transcript"
            print("PASS: same-profile restart releases lock, lists history, executes Write, preserves prior transcripts")

        try:
            if permission:
                import runpy
                check_permission = runpy.run_path(str(Path(__file__).with_name("console-fixture.py")))["check_permission"]
                check_permission(app, workspace, data, env, permission, target)
                if permission == "cancel":
                    recover_after_interruption()
                    assert not target.exists(), "Recovery replayed the cancelled tool"
                assert not handler_errors, handler_errors
                if permission != "cancel":
                    assert len(received) == 1, received
                    assert bool(received["acceptance_0"].get("is_error")) == (permission == "deny"), received
                return
            if lifecycle:
                check_lifecycle(app, workspace, data, env, lifecycle)
                recover_after_interruption()
                assert not (workspace / "after-wait.txt").exists(), "Recovery replayed the interrupted tool"
                assert not handler_errors, handler_errors
                return
            result = subprocess.run([str(app), "--data-dir", str(data), "--print",
                                     "--allowedTools", "Write,Edit,Read,Grep,Glob,Bash",
                                     "Exercise the six tools in this workspace."],
                                    # Pass the same long workspace using native extended-path syntax;
                                    # an unprefixed lpCurrentDirectory fails before the product starts.
                                    cwd=("\\\\?\\" + str(workspace)) if long_workspace else workspace,
                                    env=env, input="", capture_output=True,
                                    text=True, encoding="utf-8", errors="replace", timeout=120)
            assert not handler_errors, handler_errors
            assert result.returncode == 0, (result.returncode, result.stdout, result.stderr, received)
            assert "tools-acceptance-complete" in result.stdout, result.stdout
            assert len(received) == len(plan), received
            if short_path:
                # Native engines deliberately require approval for suspicious 8.3 paths.
                # The frontend must preserve this policy, not silently auto-approve it.
                for i in (0, 5):
                    assert received[f"acceptance_{i}"].get("is_error"), received
                    assert "did not approve" in received[f"acceptance_{i}"]["content"], received
                assert not target.exists(), "Denied write unexpectedly changed the workspace"
                assert not (workspace / "runtime tasks/probe.txt").exists()
                assert "Some tool requests were denied" in result.stdout
                print("PASS: 8.3 path policy requires approval; noninteractive deny has no write side effects")
                return
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
            if long_workspace:
                print("PASS: all six actual tools execute with workspace cwd beyond 260 characters")
        finally:
            server.shutdown()
            server.server_close()


if __name__ == "__main__":
    executable = Path(sys.argv[1]).resolve()
    check(executable)
    check(executable, long_workspace=True)
    check(executable, short_path=True)
    check(executable, lifecycle="cancel")
    check(executable, lifecycle="crash")

    for permission in ("allow", "deny", "cancel"):
        check(executable, permission=permission)
