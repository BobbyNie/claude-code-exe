"""Exercise real engine session-writer concurrency against a loopback API fixture."""

import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


SESSION_LINE = re.compile(r"(?m)^\d+\. ([0-9a-fA-F-]{36})  ")


class FixtureState:
    def __init__(self):
        self._lock = threading.Lock()
        self.requests = []
        self.errors = []
        self.block_start = 0
        self.block_expected = 0
        self.arrived = threading.Event()
        self.release = threading.Event()

    def begin_block(self, expected):
        with self._lock:
            self.block_start = len(self.requests)
            self.block_expected = expected
            self.errors.clear()
            self.arrived.clear()
            self.release.clear()

    def record_message(self, body):
        with self._lock:
            self.requests.append(body)
            blocking = self.block_expected > 0
            block_count = len(self.requests) - self.block_start
            if blocking and block_count >= self.block_expected:
                self.arrived.set()
        if blocking and not self.release.wait(45):
            with self._lock:
                self.errors.append("fixture response release timed out")

    def blocked_requests(self):
        with self._lock:
            return list(self.requests[self.block_start:])

    def finish_block(self):
        self.release.set()
        with self._lock:
            self.block_expected = 0


def stream_response(model, answer):
    message = {
        "id": "msg_concurrency_test",
        "type": "message",
        "role": "assistant",
        "model": model,
        "content": [{"type": "text", "text": answer}],
        "stop_reason": "end_turn",
        "stop_sequence": None,
        "usage": {"input_tokens": 100, "output_tokens": 5},
    }
    events = [
        ("message_start", {"message": dict(message, content=[], stop_reason=None)}),
        ("content_block_start", {"index": 0, "content_block": {"type": "text", "text": ""}}),
        ("content_block_delta", {"index": 0, "delta": {"type": "text_delta", "text": answer}}),
        ("content_block_stop", {"index": 0}),
        ("message_delta", {"delta": {"stop_reason": "end_turn", "stop_sequence": None},
                           "usage": {"output_tokens": 5}}),
        ("message_stop", {}),
    ]
    return "".join(
        f"event: {kind}\ndata: {json.dumps(dict(value, type=kind))}\n\n"
        for kind, value in events
    ).encode("utf-8")


def make_handler(state):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            size = int(self.headers.get("Content-Length", "0"))
            body = json.loads(self.rfile.read(size))
            path = self.path.split("?", 1)[0]
            if path == "/v1/messages/count_tokens":
                payload = b'{"input_tokens":100}'
                content_type = "application/json"
            elif path == "/v1/messages":
                state.record_message(body)
                payload = stream_response(body.get("model", "claude-sonnet-4-6"),
                                          "concurrency-fixture-ok")
                content_type = "text/event-stream"
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

    return Handler


def clean_environment(port):
    env = {
        key: value for key, value in os.environ.items()
        if not key.startswith(("A_", "C_", "ANTHROPIC_", "CLAUDE_", "CCODE_"))
    }
    env.update(
        A_AUTH_TOKEN="test-only-token",
        A_BASE_URL=f"http://127.0.0.1:{port}",
        NO_PROXY="127.0.0.1,localhost",
        no_proxy="127.0.0.1,localhost",
    )
    return env


def invoke(command, workspace, env, timeout=60):
    return subprocess.run(
        command,
        cwd=workspace,
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )


def session_ids(executable, data, workspace, env):
    result = invoke(
        [str(executable), "--data-dir", str(data), "--sessions"], workspace, env, timeout=30
    )
    assert result.returncode == 0, result.stdout + result.stderr
    return {match.group(1).lower() for match in SESSION_LINE.finditer(result.stdout)}


def create_session(executable, data, workspace, env, marker):
    before = session_ids(executable, data, workspace, env)
    result = invoke(
        [str(executable), "--data-dir", str(data), "--print", marker], workspace, env
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "concurrency-fixture-ok" in result.stdout, result.stdout + result.stderr
    after = session_ids(executable, data, workspace, env)
    created = after - before
    assert len(created) == 1, f"Expected one new session, got {len(created)}"
    return created.pop()


def transcript_path(data, session):
    matches = list((data / "profile").rglob(session + ".jsonl"))
    assert len(matches) == 1, f"Expected one authoritative transcript for {session}, got {len(matches)}"
    return matches[0]


def transcript_text(data, session):
    path = transcript_path(data, session)
    lines = path.read_text(encoding="utf-8").splitlines()
    parsed = [json.loads(line) for line in lines if line.strip()]
    event_sessions = {
        item["sessionId"].lower() for item in parsed
        if isinstance(item, dict) and isinstance(item.get("sessionId"), str)
    }
    assert event_sessions == {session}, f"Transcript event identity mismatch for {session}"
    return "\n".join(lines)


def assert_request_markers(requests, expected, forbidden=()):
    encoded = "\n".join(json.dumps(body, ensure_ascii=False) for body in requests)
    for marker in expected:
        assert marker in encoded, f"Missing API request marker: {marker}"
    for marker in forbidden:
        assert marker not in encoded, f"Unexpected API request marker: {marker}"


def verify(executable):
    state = FixtureState()
    server = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(state))
    server.daemon_threads = True
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    env = clean_environment(server.server_port)

    try:
        with tempfile.TemporaryDirectory(prefix="ccode-concurrency-") as temporary:
            root = Path(temporary)
            data = root / "data"
            workspace = root / "workspace"
            workspace.mkdir()
            base = [str(executable), "--data-dir", str(data)]

            first_seed = "concurrency-seed-one-314159"
            second_seed = "concurrency-seed-two-271828"
            first_session = create_session(executable, data, workspace, env, first_seed)
            first_history = transcript_text(data, first_session)
            assert first_seed in first_history
            print("PASS: actual engine accepts frontend-assigned new session identity")

            first_writer = "same-session-first-writer-161803"
            rejected_writer = "same-session-rejected-writer-141421"
            state.begin_block(1)
            primary = subprocess.Popen(
                base + ["--resume", first_session, "--print", first_writer],
                cwd=workspace,
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
            try:
                assert state.arrived.wait(30), "First same-session writer never reached the API fixture"
                snapshots = data / "snapshots"
                before_snapshot_entries = sorted(
                    item.relative_to(snapshots).as_posix() for item in snapshots.rglob("*")
                ) if snapshots.exists() else []
                maintenance = invoke(
                    base + ["--snapshot-profile"], workspace, env, timeout=30
                )
                assert maintenance.returncode == 75, maintenance.stdout + maintenance.stderr
                assert maintenance.stderr.strip() == "E_PROFILE_BUSY", maintenance.stdout + maintenance.stderr
                after_snapshot_entries = sorted(
                    item.relative_to(snapshots).as_posix() for item in snapshots.rglob("*")
                ) if snapshots.exists() else []
                assert after_snapshot_entries == before_snapshot_entries

                rejected = invoke(
                    base + ["--resume", first_session, "--print", rejected_writer],
                    workspace,
                    env,
                    timeout=30,
                )
                assert rejected.returncode == 75, rejected.stdout + rejected.stderr
                assert rejected.stderr.strip() == "E_SESSION_BUSY", rejected.stdout + rejected.stderr
                assert not rejected.stdout.strip(), rejected.stdout
                blocked = state.blocked_requests()
                assert len(blocked) == 1, "Rejected same-session writer issued a model request"
                assert_request_markers(blocked, [first_writer], [rejected_writer])
            finally:
                state.finish_block()
                primary_stdout, primary_stderr = primary.communicate(timeout=60)
            assert primary.returncode == 0, primary_stdout + primary_stderr
            assert not state.errors, state.errors
            same_history = transcript_text(data, first_session)
            assert first_seed in same_history and first_writer in same_history
            assert rejected_writer not in same_history
            print("PASS: second writer for the same real session exits 75 before any model request or transcript write")

            second_session = create_session(executable, data, workspace, env, second_seed)
            assert second_session != first_session
            assert second_seed in transcript_text(data, second_session)

            parallel_first = "parallel-session-one-173205"
            parallel_second = "parallel-session-two-223606"
            state.begin_block(2)
            processes = [
                subprocess.Popen(
                    base + ["--resume", session, "--print", marker],
                    cwd=workspace,
                    env=env,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                )
                for session, marker in (
                    (first_session, parallel_first),
                    (second_session, parallel_second),
                )
            ]
            try:
                assert state.arrived.wait(30), (
                    "Two different sessions did not both reach the API before either response was released"
                )
                blocked = state.blocked_requests()
                assert len(blocked) == 2, f"Expected two parallel model requests, got {len(blocked)}"
                assert_request_markers(blocked, [parallel_first, parallel_second])
            finally:
                state.finish_block()
                outputs = [process.communicate(timeout=60) for process in processes]
            for process, (stdout, stderr) in zip(processes, outputs):
                assert process.returncode == 0, stdout + stderr
            assert not state.errors, state.errors

            first_history = transcript_text(data, first_session)
            second_history = transcript_text(data, second_session)
            assert first_seed in first_history and first_writer in first_history and parallel_first in first_history
            assert second_seed not in first_history and parallel_second not in first_history
            assert second_seed in second_history and parallel_second in second_history
            assert first_seed not in second_history and first_writer not in second_history and parallel_first not in second_history
            print("PASS: two real sessions in one workspace run concurrently without overwriting histories")
    finally:
        state.finish_block()
        server.shutdown()
        server.server_close()
        worker.join(timeout=5)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: concurrency-integration.py CCODE_EXE")
    executable = Path(sys.argv[1]).resolve()
    if os.name != "nt":
        raise SystemExit("This acceptance fixture requires Windows")
    verify(executable)
