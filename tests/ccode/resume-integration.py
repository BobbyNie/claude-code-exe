"""Exercise real --resume against a local fake API, without user credentials."""
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import threading
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


@contextmanager
def deny_pointer_replacement(pointer):
    """Real Windows sharing violation: allow reads/writes, forbid delete/rename."""
    import ctypes
    from ctypes import wintypes as w
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, ctypes.c_void_p,
                                  w.DWORD, w.DWORD, w.HANDLE]
    kernel.CreateFileW.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.CloseHandle.restype = w.BOOL
    handle = kernel.CreateFileW(str(pointer), 0x80000000, 3, None, 3, 0, None)
    assert handle != ctypes.c_void_p(-1).value, "Cannot establish pointer replacement lock"
    try:
        yield
    finally:
        assert kernel.CloseHandle(handle), "Cannot release pointer replacement lock"


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

    # The all-session rejection case must not depend on the PowerShell caller
    # having installed its separate migration-conflict fixtures first.
    invalid_project = root / "data/cc/profile/home/.cc/projects/invalid-history-fixture"
    invalid_project.mkdir(parents=True, exist_ok=True)
    with (invalid_project / "invalid-session.jsonl").open("x", encoding="utf-8") as output:
        output.write("invalid-history-fixture\n")



def fixture_answer(messages, history_markers, probe_reply_correct=True):
    # Native engines may append system context after the current user turn.
    # Locate the final user message, never a historical probe or a system hint.
    user_index = next((index for index in range(len(messages) - 1, -1, -1)
                       if messages[index].get("role") == "user"), None)
    if user_index is None:
        return "resume-test-ok"
    current = json.dumps(messages[user_index].get("content"))
    if "For profile recovery verification" not in current:
        return "resume-test-ok"
    assert not any(marker in current for marker in history_markers), "Probe leaked the expected answer"
    history = json.dumps(messages[:user_index])
    recovered = [marker for marker in history_markers if marker in history]
    return recovered[0] if probe_reply_correct and len(recovered) == 1 else "history-verification-failed"


def verify(executable, previous_executable=None):
    requests = []
    probe_reply_correct = True
    history_markers = ["legacy-resume-marker-7391", "second-workspace-marker-6842"]

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
            answer = fixture_answer(body.get("messages", []), history_markers, probe_reply_correct)
            message = {"id": "msg_resume_test", "type": "message", "role": "assistant",
                       "model": body.get("model", "claude-sonnet-4-6"),
                       "content": [{"type": "text", "text": answer}],
                       "stop_reason": "end_turn", "stop_sequence": None,
                       "usage": {"input_tokens": 100, "output_tokens": 5}}
            if body.get("stream"):
                events = [
                    ("message_start", {"message": dict(message, content=[], stop_reason=None)}),
                    ("content_block_start", {"index": 0, "content_block": {"type": "text", "text": ""}}),
                    ("content_block_delta", {"index": 0, "delta": {"type": "text_delta", "text": answer}}),
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
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("A_", "C_", "ANTHROPIC_", "CLAUDE_", "CCODE_"))}
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
        legacy = executable.parent / "data/cc/profile/home/.cc/projects"
        originals = {path: path.read_bytes() for path in legacy.rglob("*.jsonl")}
        assert originals, "Missing authoritative legacy fixture"
        listed = subprocess.run([str(executable), "--sessions"], cwd=executable.parent,
                                env=env, capture_output=True, text=True, encoding="utf-8", timeout=30)
        assert listed.returncode == 0, listed.stderr
        # Derive the selection from the actual public list, not a guessed index.
        selection = re.search(r"(?m)^(\d+)\. " + re.escape(session) + r"  ", listed.stdout)
        assert selection, "Known session absent from public history list"
        profile = executable.parent / "data/cc/profile"
        identity = subprocess.run([str(executable), "--workspace-id"], cwd=executable.parent,
                                  env=env, capture_output=True, text=True, encoding="utf-8", timeout=30)
        assert identity.returncode == 0, identity.stderr
        workspace_id = str(uuid.UUID(identity.stdout.strip()))
        index_path = profile / "session-index" / (workspace_id + ".json")
        index = json.loads(index_path.read_text(encoding="utf-8"))
        assert index["workspaceId"] == workspace_id
        assert any(item["id"] == session and item["availability"] == "discovered"
                   for item in index["sessions"]), "Workspace index omitted authoritative session"
        native_files = {path: path.read_bytes() for path in (profile / "home/.claude/projects").rglob("*.jsonl")}
        assert native_files
        for damage in ("corrupt", "delete"):
            if damage == "corrupt":
                index_path.write_text('{"sessions":[{"id":"fabricated-cache-session"}]}', encoding="utf-8")
            else:
                index_path.unlink()
            rebuilt = subprocess.run([str(executable), "--sessions"], cwd=executable.parent,
                                     env=env, capture_output=True, text=True, encoding="utf-8", timeout=30)
            assert rebuilt.returncode == 0 and session in rebuilt.stdout, rebuilt
            assert "fabricated-cache-session" not in rebuilt.stdout
            assert json.loads(index_path.read_text(encoding="utf-8")) == index
            assert all(path.read_bytes() == saved for path, saved in native_files.items())
        print("PASS: workspace UUID session index rebuilds from unchanged native history, never stale cache")
        damaged_id = str(uuid.uuid4())
        damaged_path = next(iter(native_files)).parent / (damaged_id + ".jsonl")
        damaged_bytes = (json.dumps({"type": "user", "sessionId": damaged_id,
            "cwd": str(executable.parent), "message": {"content": "private-damaged-title"}})
            + "\n{truncated-private-marker").encode("utf-8")
        damaged_path.write_bytes(damaged_bytes)
        mixed_list = subprocess.run([str(executable), "--sessions"], cwd=executable.parent,
            env=env, capture_output=True, text=True, encoding="utf-8", timeout=30)
        assert mixed_list.returncode == 0, mixed_list.stderr
        assert damaged_id in mixed_list.stdout and "[unavailable: E_SESSION_DATA]" in mixed_list.stdout
        assert "private-damaged-title" not in mixed_list.stdout
        damaged_selection = re.search(r"(?m)^(\d+)\. " + re.escape(damaged_id) + r"  ", mixed_list.stdout)
        selection = re.search(r"(?m)^(\d+)\. " + re.escape(session) + r"  ", mixed_list.stdout)
        assert selection and damaged_selection
        requests.clear()
        refused = subprocess.run([str(executable), "--resume"], cwd=executable.parent,
            env=env, input=f"{damaged_selection[1]}\n/exit\n", capture_output=True,
            text=True, encoding="utf-8", timeout=60)
        assert refused.returncode == 0 and "E_SESSION_DATA" in refused.stdout, refused
        assert "Selected " + damaged_id not in refused.stdout and not requests
        first = "picker-first-turn-marker-4816"
        second = "picker-second-turn-marker-8527"
        requests.clear()
        picked = subprocess.run([str(executable), "--resume"], cwd=executable.parent,
                                env=env, input=f"{selection[1]}\n{first}\n{second}\n/exit\n",
                                capture_output=True, text=True, encoding="utf-8", timeout=120)
        assert picked.returncode == 0, picked.stdout + picked.stderr
        assert "Selected " + session in picked.stdout, picked.stdout
        assert picked.stdout.count("resume-test-ok") >= 2, picked.stdout

        def has_history(current, required):
            # Inspect actual upstream messages; UI labels alone cannot prove resume.
            for request in requests:
                messages = request.get("messages", [])
                if not messages or current not in json.dumps(messages[-1]):
                    continue
                history = json.dumps(messages[:-1])
                if all(marker in history for marker in required):
                    return True
            return False

        assert has_history(first, ["legacy-resume-marker-7391"]), "Picker lost historical context"
        assert has_history(second, ["legacy-resume-marker-7391", first]), "Second turn lost session context"
        print("PASS: public history picker resumes real engine and preserves context across two turns")
        assert damaged_path.read_bytes() == damaged_bytes
        damaged_path.unlink()  # Remove this fixture before unrelated migration acceptance.
        print("PASS: unavailable session cannot be selected; healthy history still resumes actual context without changing damaged bytes")
        requests.clear()
        continued = "continue-after-restart-marker-9638"
        result = subprocess.run([str(executable), "--continue", "--print", continued],
                                cwd=executable.parent, env=env, capture_output=True,
                                text=True, encoding="utf-8", timeout=60)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "resume-test-ok" in result.stdout, result.stdout
        assert has_history(continued, ["legacy-resume-marker-7391", first, second]), "Continue lost saved turns"
        assert all(path.read_bytes() == saved for path, saved in originals.items()), "Legacy source changed"
        print("PASS: restart and continue load both saved turns without modifying legacy source")
        def profile_bytes(folder):
            return {path.relative_to(folder).as_posix(): path.read_bytes()
                    for path in folder.rglob("*") if path.is_file() and path.name != "frontend.lock"}

        active_before = profile_bytes(profile)
        backup = subprocess.run([str(executable), "--snapshot-profile"], cwd=executable.parent,
                                env=env, capture_output=True, text=True, encoding="utf-8", timeout=30)
        if backup.returncode != 0:
            # Fixture-only structural diagnostics: no path names or file contents.
            pending = list((profile.parent / "snapshots").glob("*.pending"))
            missing = [(profile / name, folder / "profile" / name)
                       for folder in pending for name in active_before
                       if not (folder / "profile" / name).is_file()]
            print("Snapshot fixture diagnostics:", json.dumps({
                "pending_count": len(pending), "missing_count": len(missing),
                "missing_source_exists": all(src.is_file() for src, _ in missing),
                "max_source_chars": max((len(str(src)) for src, _ in missing), default=0),
                "min_missing_destination_chars": min((len(str(dst)) for _, dst in missing), default=0),
                "max_destination_chars": max((len(str(dst)) for _, dst in missing), default=0),
            }), flush=True)
        assert backup.returncode == 0, backup.stderr
        snapshot_id = str(uuid.UUID(backup.stdout.strip()))
        snapshot = profile.parent / "snapshots" / snapshot_id
        snapshot_before = profile_bytes(snapshot)
        staged = subprocess.run([str(executable), "--stage-profile", snapshot_id], cwd=executable.parent,
                                env=env, capture_output=True, text=True, encoding="utf-8", timeout=30)
        assert staged.returncode == 0, staged.stderr
        candidate_id = str(uuid.UUID(staged.stdout.strip()))
        candidate = profile.parent / "candidates" / candidate_id
        candidate_before = profile_bytes(candidate / "profile")
        assert candidate_before == active_before, "Candidate is not an exact profile copy"
        requests.clear()
        candidate_prompt = "isolated-candidate-resume-marker-1749"
        restored = subprocess.run([str(executable), "--data-dir", str(candidate),
                                   "--resume", session, "--print", candidate_prompt],
                                  cwd=executable.parent, env=env, capture_output=True,
                                  text=True, encoding="utf-8", timeout=60)
        assert restored.returncode == 0, restored.stdout + restored.stderr
        assert "resume-test-ok" in restored.stdout
        assert has_history(candidate_prompt, ["legacy-resume-marker-7391", first, second, continued]), \
            "Candidate engine request lost historical context"
        assert profile_bytes(profile) == active_before, "Candidate validation changed active profile"
        assert profile_bytes(snapshot) == snapshot_before, "Candidate validation changed backup"
        candidate_after = profile_bytes(candidate / "profile")
        assert any(candidate_prompt.encode() in content for name, content in candidate_after.items()
                   if name.endswith(".jsonl")), "Restored turn was not persisted in candidate"
        assert json.loads((candidate / "candidate.json").read_text(encoding="utf-8"))["state"] == "staged"
        assert not (profile.parent / "active-profile.json").exists()
        print("PASS: actual engine resumes isolated candidate history while active profile and backup remain byte-identical")
        # The product verifier, unlike the previous raw resume, publishes bound evidence.
        def stage_again():
            result = subprocess.run([str(executable), "--stage-profile", snapshot_id], cwd=executable.parent,
                                    env=env, capture_output=True, text=True, encoding="utf-8", timeout=30)
            assert result.returncode == 0, result.stderr
            return str(uuid.UUID(result.stdout.strip()))

        conflict_id = stage_again()
        conflict_root = profile.parent / "candidates" / conflict_id
        candidate_before_conflict = profile_bytes(conflict_root)
        changed_source = profile / "new-source-data.txt"
        changed_source.write_bytes(b"preserve source changes")
        requests.clear()
        conflict = subprocess.run([str(executable), "--validate-profile", conflict_id, "--resume", session],
                                  cwd=executable.parent, env=env, capture_output=True,
                                  text=True, encoding="utf-8", timeout=60)
        assert conflict.returncode == 64 and conflict.stderr.strip() == "E_SOURCE_CHANGED", conflict
        assert not requests, "Source conflict must be rejected before any API request"
        assert changed_source.read_bytes() == b"preserve source changes"
        assert profile_bytes(conflict_root) == candidate_before_conflict
        assert profile_bytes(snapshot) == snapshot_before
        assert not (profile.parent / "active-profile.json").exists()
        changed_source.unlink()
        assert profile_bytes(profile) == active_before
        print("PASS: changed active source rejects candidate validation before API without changing source, backup or candidate")
        validation_id = stage_again()
        requests.clear()
        validated = subprocess.run([str(executable), "--validate-profile", validation_id, "--resume", session],
                                   cwd=executable.parent, env=env, capture_output=True,
                                   text=True, encoding="utf-8", timeout=60)
        assert validated.returncode == 0, validated.stdout + validated.stderr
        assert "legacy-resume-marker-7391" not in validated.stdout + validated.stderr
        assert len(requests) == 1, "Verifier replayed a model request"
        assert has_history("For profile recovery verification", ["legacy-resume-marker-7391", first, second, continued])
        validated_root = profile.parent / "candidates" / validation_id
        receipt = json.loads((validated_root / "validation.json").read_text(encoding="utf-8"))
        assert receipt["scope"] == "single-session" and receipt["historyVerified"] is True
        assert receipt["candidateId"] == validation_id and receipt["sourceSnapshotId"] == snapshot_id
        assert receipt["sessionId"] == session and receipt["workspaceId"] == workspace_id
        assert len(receipt["engine"]["sha256"]) == 64 and receipt["engine"]["version"]
        assert "legacy-resume-marker-7391" not in json.dumps(receipt)
        frozen = profile.parent / "verified" / str(uuid.UUID(receipt["verificationId"]))
        manifest = json.loads((frozen / "manifest.json").read_text(encoding="utf-8"))
        assert receipt["files"] == manifest["files"]
        saved = profile_bytes(validated_root / "profile")
        assert saved == profile_bytes(frozen / "profile")
        assert set(saved) == set(receipt["files"])
        for name, value in saved.items():
            assert receipt["files"][name] == {"sha256": hashlib.sha256(value).hexdigest(), "size": len(value)}
        assert profile_bytes(profile) == active_before and profile_bytes(snapshot) == snapshot_before
        failed_id = stage_again()
        probe_reply_correct = False
        requests.clear()
        failed = subprocess.run([str(executable), "--validate-profile", failed_id, "--resume", session],
                                cwd=executable.parent, env=env, capture_output=True,
                                text=True, encoding="utf-8", timeout=60)
        assert failed.returncode == 64 and failed.stderr.strip() == "E_CANDIDATE_HISTORY", failed
        assert len(requests) == 1 and not (profile.parent / "candidates" / failed_id / "validation.json").exists()
        assert profile_bytes(profile) == active_before and profile_bytes(snapshot) == snapshot_before
        assert not (profile.parent / "active-profile.json").exists()
        print("PASS: product candidate verifier checks real recovered history, freezes SHA256-bound evidence, and rejects a wrong answer without replay or activation")

        # The legacy migration fixture deliberately contains invalid JSONL filenames.
        # All-session validation must reject that profile rather than skip those files.
        mixed_id = stage_again()
        mixed_root = profile.parent / "candidates" / mixed_id
        mixed_before = profile_bytes(mixed_root)
        requests.clear()
        mixed_result = subprocess.run([str(executable), "--validate-profile", mixed_id, "--all-sessions"],
                                      cwd=executable.parent, env=env, capture_output=True,
                                      text=True, encoding="utf-8", timeout=60)
        assert mixed_result.returncode == 64 and mixed_result.stderr.strip() == "E_CANDIDATE_HISTORY", mixed_result
        assert not requests and profile_bytes(mixed_root) == mixed_before
        assert profile_bytes(profile) == active_before and profile_bytes(snapshot) == snapshot_before
        print("PASS: all-session preflight rejects mixed invalid legacy transcripts without API requests or data changes")

        # Use a separate profile of real engine-created sessions for the success case.
        # Do not delete or rewrite the deliberately invalid original migration fixture.
        probe_reply_correct = True
        all_data = executable.parent / "all sessions data"
        all_env = dict(env, CCODE_DATA_DIR=str(all_data))
        other_workspace = executable.parent / "second 工作區"
        other_workspace.mkdir()
        real_sessions = set()
        workspace_sessions = {}
        for workspace, marker in zip((executable.parent, other_workspace), history_markers):
            created = subprocess.run([str(executable), "--print", marker], cwd=workspace,
                                     env=all_env, capture_output=True, text=True, encoding="utf-8", timeout=60)
            assert created.returncode == 0, created
            listed_all = subprocess.run([str(executable), "--sessions"], cwd=workspace,
                                        env=all_env, capture_output=True, text=True, encoding="utf-8", timeout=30)
            assert listed_all.returncode == 0, listed_all
            match = re.search(r"(?m)^\d+\. ([0-9a-f-]{36})  ", listed_all.stdout)
            assert match, listed_all.stdout
            real_sessions.add(str(uuid.UUID(match[1])))
            workspace_sessions[workspace] = str(uuid.UUID(match[1]))
        assert len(real_sessions) == 2
        all_profile = all_data / "profile"
        active_all_before = profile_bytes(all_profile)
        all_backup = subprocess.run([str(executable), "--snapshot-profile"], cwd=executable.parent,
                                    env=all_env, capture_output=True, text=True, encoding="utf-8", timeout=30)
        assert all_backup.returncode == 0, all_backup
        all_snapshot_id = str(uuid.UUID(all_backup.stdout.strip()))
        all_snapshot = all_data / "snapshots" / all_snapshot_id
        all_snapshot_before = profile_bytes(all_snapshot)
        all_staged = subprocess.run([str(executable), "--stage-profile", all_snapshot_id], cwd=executable.parent,
                                    env=all_env, capture_output=True, text=True, encoding="utf-8", timeout=30)
        assert all_staged.returncode == 0, all_staged
        all_id = str(uuid.UUID(all_staged.stdout.strip()))
        requests.clear()
        all_result = subprocess.run([str(executable), "--validate-profile", all_id, "--all-sessions"],
                                    cwd=executable.parent, env=all_env, capture_output=True,
                                    text=True, encoding="utf-8", timeout=120)
        if all_result.returncode != 0:
            # Synthetic fixture diagnostics only: never print transcript/prompt contents.
            diagnostics = {"api_requests": len(requests), "requests": [], "transcripts": []}
            for request in requests:
                messages = request.get("messages", [])
                diagnostics["requests"].append({
                    "history_markers": [marker in json.dumps(messages[:-1]) for marker in history_markers],
                    "probe_last": bool(messages) and "For profile recovery verification" in json.dumps(messages[-1]),
                    "message_shapes": [{"role": message.get("role"),
                        "probe": "For profile recovery verification" in json.dumps(message),
                        "markers": [marker in json.dumps(message) for marker in history_markers],
                        "content_kind": type(message.get("content")).__name__,
                        "block_types": [block.get("type") for block in message.get("content", [])
                                        if isinstance(block, dict)]
                                        if isinstance(message.get("content"), list) else []}
                        for message in messages]})
            candidate_profile = all_data / "candidates" / all_id / "profile"
            for transcript in sorted((candidate_profile / "home/.claude/projects").glob("*/*.jsonl")):
                info = {"known_id": transcript.stem in real_sessions, "records": []}
                for line in transcript.read_text(encoding="utf-8").splitlines():
                    try:
                        event = json.loads(line)
                    except ValueError:
                        info["records"].append({"valid_json": False})
                        continue
                    if not isinstance(event, dict):
                        info["records"].append({"object": False})
                        continue
                    if event.get("type") != "user":
                        continue
                    message = event.get("message")
                    content = message.get("content") if isinstance(message, dict) else None
                    text = content if isinstance(content, str) else "".join(
                        block["text"] for block in content
                        if isinstance(block, dict) and isinstance(block.get("text"), str)) if isinstance(content, list) else ""
                    cwd = event.get("cwd")
                    info["records"].append({
                        "id_matches": event.get("sessionId") == transcript.stem,
                        "cwd_known": isinstance(cwd, str) and any(
                            os.path.normcase(str(Path(cwd).resolve())) == os.path.normcase(str(path.resolve()))
                            for path in (executable.parent, other_workspace)),
                        "sidechain": event.get("isSidechain"), "content_kind": type(content).__name__,
                        "block_types": [block.get("type") if isinstance(block, dict) else "invalid"
                                        for block in content] if isinstance(content, list) else [],
                        "exact_markers": [text == marker for marker in history_markers],
                        "probe": "For profile recovery verification" in text})
                diagnostics["transcripts"].append(info)
            print("ALL_SESSION_DIAGNOSTICS " + json.dumps(diagnostics), flush=True)
        assert all_result.returncode == 0, all_result
        assert "Verified candidate sessions: 2" in all_result.stdout
        assert not any(marker in all_result.stdout + all_result.stderr for marker in history_markers)
        assert len(requests) == 2, "All-session validation omitted or replayed a session"
        for marker in history_markers:
            assert sum(marker in json.dumps(request.get("messages", [])[:-1]) for request in requests) == 1
        all_root = all_data / "candidates" / all_id
        all_receipt = json.loads((all_root / "validation.json").read_text(encoding="utf-8"))
        assert all_receipt["scope"] == "all-top-level-sessions"
        assert {item["sessionId"] for item in all_receipt["sessions"]} == real_sessions
        assert len({item["workspaceId"] for item in all_receipt["sessions"]}) == 2
        assert not any(marker in json.dumps(all_receipt) for marker in history_markers)
        all_saved = profile_bytes(all_root / "profile")
        all_frozen = all_data / "verified" / str(uuid.UUID(all_receipt["verificationId"]))
        assert all_saved == profile_bytes(all_frozen / "profile")
        assert all_receipt["files"] == {name: {"size": len(value), "sha256": hashlib.sha256(value).hexdigest()}
                                         for name, value in all_saved.items()}
        assert profile_bytes(all_profile) == active_all_before
        assert profile_bytes(all_snapshot) == all_snapshot_before
        assert profile_bytes(profile) == active_before and profile_bytes(snapshot) == snapshot_before
        assert not (all_data / "active-profile.json").exists()
        print("PASS: all-session candidate validation restores two real sessions in distinct workspaces with complete SHA256-bound receipt and unchanged source")
        requests.clear()
        activation_args = [str(executable), "--activate-profile", all_id]
        conflict = all_profile / "after-validation.txt"
        conflict.write_text("preserve later source data", encoding="utf-8")
        blocked = subprocess.run(activation_args, cwd=executable.parent, env=all_env,
                                 capture_output=True, text=True, encoding="utf-8", timeout=60)
        assert blocked.returncode == 64 and blocked.stderr.strip() == "E_SOURCE_CHANGED", blocked
        assert conflict.read_text(encoding="utf-8") == "preserve later source data"
        assert not (all_data / "active-profile.json").exists() and not requests
        conflict.unlink()
        activated = subprocess.run(activation_args, cwd=executable.parent, env=all_env,
                                   capture_output=True, text=True, encoding="utf-8", timeout=60)
        assert activated.returncode == 0, activated
        pointer = json.loads((all_data / "active-profile.json").read_text(encoding="utf-8"))
        assert pointer["candidateId"] == all_id and pointer["verificationId"] == all_receipt["verificationId"]
        assert not requests and not (all_data / "active-profile.json.pending").exists()
        assert profile_bytes(all_profile) == active_all_before
        assert profile_bytes(all_snapshot) == all_snapshot_before
        for workspace, marker in zip((executable.parent, other_workspace), history_markers):
            resumed = subprocess.run([str(executable), "--continue", "--print", "after activation"],
                                     cwd=workspace, env=all_env, capture_output=True, text=True,
                                     encoding="utf-8", timeout=60)
            assert resumed.returncode == 0, resumed
            assert marker in json.dumps(requests[-1].get("messages", [])), "Activated profile lost native history"
        assert profile_bytes(all_profile) == active_all_before
        assert profile_bytes(all_snapshot) == all_snapshot_before
        assert profile_bytes(all_frozen / "profile") == all_saved
        assert profile_bytes(all_root / "profile") != all_saved, "New turns must write selected live profile"
        print("PASS: activation rejects changed source, commits verified pointer without API, and restarts both workspaces with preserved history and unchanged backups")
        # Exercise replacement of an existing pointer, not just its first creation.
        def active_command(*arguments):
            result = subprocess.run([str(executable), *arguments], cwd=executable.parent,
                                    env=all_env, capture_output=True, text=True, encoding="utf-8", timeout=120)
            assert result.returncode == 0, result
            return result

        live_before_replacement = profile_bytes(all_root / "profile")
        next_snapshot_id = str(uuid.UUID(active_command("--snapshot-profile").stdout.strip()))
        next_id = str(uuid.UUID(active_command("--stage-profile", next_snapshot_id).stdout.strip()))
        active_command("--validate-profile", next_id, "--all-sessions")
        old_pointer = (all_data / "active-profile.json").read_bytes()
        pending_pointer = all_data / "active-profile.json.pending"
        pending_pointer.write_text("interrupted replacement evidence", encoding="utf-8")
        requests.clear()
        interrupted = subprocess.run([str(executable), "--activate-profile", next_id],
                                     cwd=executable.parent, env=all_env, capture_output=True,
                                     text=True, encoding="utf-8", timeout=60)
        assert interrupted.returncode == 64 and interrupted.stderr.strip() == "E_ACTIVATION_PENDING", interrupted
        assert (all_data / "active-profile.json").read_bytes() == old_pointer
        assert pending_pointer.read_text(encoding="utf-8") == "interrupted replacement evidence"
        assert profile_bytes(all_root / "profile") == live_before_replacement and not requests
        recovery_id = str(uuid.UUID(active_command("--archive-activation-pending").stdout.strip()))
        archived_pending = all_data / "activation-recovery" / recovery_id / "pending.json"
        assert archived_pending.read_bytes() == b"interrupted replacement evidence"
        assert not pending_pointer.exists()
        assert (all_data / "active-profile.json").read_bytes() == old_pointer
        assert not requests
        # An OS sharing violation exercises the actual commit failure path,
        # unlike a manually created pending file. No product fault-injection flag.
        frozen_before_failure = profile_bytes(all_data / "verified")
        snapshots_before_failure = profile_bytes(all_data / "snapshots")
        candidate_before_failure = profile_bytes(all_data / "candidates" / next_id / "profile")
        with deny_pointer_replacement(all_data / "active-profile.json"):
            failed_commit = subprocess.run([str(executable), "--activate-profile", next_id],
                                           cwd=executable.parent, env=all_env, capture_output=True,
                                           text=True, encoding="utf-8", timeout=60)
        assert failed_commit.returncode == 64 and failed_commit.stderr.strip() == "E_ACTIVATION_WRITE", failed_commit
        assert (all_data / "active-profile.json").read_bytes() == old_pointer
        failed_pending = pending_pointer.read_bytes()
        assert json.loads(failed_pending)["candidateId"] == next_id
        assert profile_bytes(all_root / "profile") == live_before_replacement
        assert profile_bytes(all_data / "candidates" / next_id / "profile") == candidate_before_failure
        assert profile_bytes(all_data / "verified") == frozen_before_failure
        assert profile_bytes(all_data / "snapshots") == snapshots_before_failure
        assert not requests
        retry_before_recovery = subprocess.run([str(executable), "--activate-profile", next_id],
                                               cwd=executable.parent, env=all_env, capture_output=True,
                                               text=True, encoding="utf-8", timeout=60)
        assert retry_before_recovery.returncode == 64 and retry_before_recovery.stderr.strip() == "E_ACTIVATION_PENDING", retry_before_recovery
        assert pending_pointer.read_bytes() == failed_pending
        failure_archive_id = str(uuid.UUID(active_command("--archive-activation-pending").stdout.strip()))
        assert (all_data / "activation-recovery" / failure_archive_id / "pending.json").read_bytes() == failed_pending
        assert (all_data / "active-profile.json").read_bytes() == old_pointer
        active_command("--activate-profile", next_id)
        assert json.loads((all_data / "active-profile.json").read_text(encoding="utf-8"))["candidateId"] == next_id
        assert profile_bytes(all_root / "profile") == live_before_replacement
        assert profile_bytes(all_profile) == active_all_before and not requests
        print("PASS: real Windows pointer replacement failure preserves committed state and pending evidence; explicit archive permits verified retry without API")
        repeated_pointer = (all_data / "active-profile.json").read_bytes()
        repeated = subprocess.run([str(executable), "--activate-profile", next_id], cwd=executable.parent,
                                  env=all_env, capture_output=True, text=True, encoding="utf-8", timeout=60)
        assert repeated.returncode == 64 and repeated.stderr.strip() == "E_ACTIVATION_ALREADY_ACTIVE", repeated
        assert (all_data / "active-profile.json").read_bytes() == repeated_pointer
        print("PASS: second activation preserves old pointer on pending conflict, replaces it on success, and rejects reactivation without data changes")
        # Synthetic engine-identity mismatch checks the management-only boundary;
        # it is NOT evidence of actual cross-version history compatibility.
        foreign_data = executable.parent / "foreign engine backup data"
        shutil.copytree(all_data, foreign_data)
        foreign_pointer_path = foreign_data / "active-profile.json"
        foreign_pointer = json.loads(foreign_pointer_path.read_text(encoding="utf-8"))
        foreign_pointer["engine"] = {"version": "0.0.0-fixture", "sha256": "f" * 64}
        foreign_pointer_path.write_text(json.dumps(foreign_pointer), encoding="utf-8")
        foreign_candidate = foreign_data / "candidates" / next_id
        foreign_receipt_path = foreign_candidate / "validation.json"
        foreign_receipt = json.loads(foreign_receipt_path.read_text(encoding="utf-8"))
        foreign_receipt["engine"] = foreign_pointer["engine"]
        foreign_receipt_path.write_text(json.dumps(foreign_receipt), encoding="utf-8")
        foreign_env = {key: value for key, value in all_env.items()
                       if not key.startswith(("A_", "ANTHROPIC_", "CLAUDE_"))}
        foreign_env["CCODE_DATA_DIR"] = str(foreign_data)
        def foreign_command(*arguments):
            return subprocess.run([str(executable), *arguments], cwd=executable.parent,
                                  env=foreign_env, capture_output=True, text=True, encoding="utf-8", timeout=60)
        foreign_before = profile_bytes(foreign_candidate / "profile")
        pointer_before = foreign_pointer_path.read_bytes()
        runtime_refused = foreign_command("--sessions")
        assert runtime_refused.returncode == 64 and runtime_refused.stderr.strip() == "E_ACTIVE_PROFILE", runtime_refused
        preserved = foreign_command("--snapshot-profile")
        assert preserved.returncode == 0, preserved
        preserved_id = str(uuid.UUID(preserved.stdout.strip()))
        preserved_snapshot = foreign_data / "snapshots" / preserved_id
        assert profile_bytes(preserved_snapshot / "profile") == foreign_before
        manifest = json.loads((preserved_snapshot / "manifest.json").read_text(encoding="utf-8"))
        assert set(manifest["files"]) == set(foreign_before)
        for name, content in foreign_before.items():
            assert manifest["files"][name] == {"sha256": hashlib.sha256(content).hexdigest(), "size": len(content)}
        assert profile_bytes(foreign_candidate / "profile") == foreign_before
        assert foreign_pointer_path.read_bytes() == pointer_before and not requests
        runtime_refused = foreign_command("--sessions")
        assert runtime_refused.returncode == 64 and runtime_refused.stderr.strip() == "E_ACTIVE_PROFILE", runtime_refused
        # An older source is explicitly chosen; recent active history remains preserved.
        old_source = foreign_data / "snapshots" / all_snapshot_id
        old_source_before = profile_bytes(old_source)
        assert profile_bytes(old_source / "profile") != foreign_before
        prepared = foreign_command("--prepare-rollback", all_snapshot_id)
        assert prepared.returncode == 0, prepared
        plan = json.loads(prepared.stdout)
        assert plan["schema"] == 1 and plan["state"] == "prepared"
        rollback_id = str(uuid.UUID(plan["candidateId"]))
        preservation_id = str(uuid.UUID(plan["preservationSnapshotId"]))
        assert plan["sourceSnapshotId"] == all_snapshot_id
        assert plan["priorActivePointer"] == foreign_pointer
        assert plan["targetEngine"] == json.loads(repeated_pointer)["engine"]
        assert plan["adapter"] == "stream-json-v1"
        rollback_candidate = foreign_data / "rollback-candidates" / rollback_id
        assert json.loads((rollback_candidate / "rollback.json").read_text(encoding="utf-8")) == plan
        assert profile_bytes(rollback_candidate / "profile") == profile_bytes(old_source / "profile")
        assert profile_bytes(foreign_data / "snapshots" / preservation_id / "profile") == foreign_before
        assert profile_bytes(foreign_candidate / "profile") == foreign_before
        assert profile_bytes(old_source) == old_source_before
        assert foreign_pointer_path.read_bytes() == pointer_before and not requests
        assert not (foreign_data / "candidates" / rollback_id).exists()
        assert not (rollback_candidate / "validation.json").exists()
        print("PASS: explicit rollback preparation preserves latest active data and pointer, isolates old snapshot, and needs no API")
        no_credential = foreign_command("--validate-rollback", rollback_id)
        assert no_credential.returncode == 64 and no_credential.stderr.startswith("E_CREDENTIAL:"), no_credential
        assert not (rollback_candidate / "rollback-validation.json").exists() and not requests
        rollback_env = dict(all_env, CCODE_DATA_DIR=str(foreign_data))
        rollback_result = subprocess.run([str(executable), "--validate-rollback", rollback_id],
            cwd=executable.parent, env=rollback_env, capture_output=True, text=True, encoding="utf-8", timeout=120)
        assert rollback_result.returncode == 0, rollback_result
        rollback_receipt = json.loads((rollback_candidate / "rollback-validation.json").read_text(encoding="utf-8"))
        assert rollback_receipt["plan"] == plan
        rollback_validation = rollback_receipt["validation"]
        assert rollback_validation["scope"] == "all-top-level-sessions"
        assert rollback_validation["engine"] == plan["targetEngine"]
        assert {item["sessionId"] for item in rollback_validation["sessions"]} == set(real_sessions)
        assert len(requests) == len(real_sessions)
        for marker in history_markers:
            assert any(marker in json.dumps(request.get("messages", [])[:-1]) for request in requests)
        assert profile_bytes(foreign_candidate / "profile") == foreign_before
        assert profile_bytes(old_source) == old_source_before
        assert profile_bytes(foreign_data / "snapshots" / preservation_id / "profile") == foreign_before
        assert foreign_pointer_path.read_bytes() == pointer_before
        requests.clear()
        duplicate = subprocess.run([str(executable), "--validate-rollback", rollback_id],
            cwd=executable.parent, env=rollback_env, capture_output=True, text=True, encoding="utf-8", timeout=60)
        assert duplicate.returncode == 64 and duplicate.stderr.strip() == "E_ROLLBACK_EXISTS", duplicate
        assert not requests
        print("PASS: public rollback verifier restores all real sessions with target engine, preserves current data, and refuses replay")
        valid_foreign_receipt = foreign_receipt_path.read_bytes()
        foreign_receipt["verificationId"] = str(uuid.uuid4())
        foreign_receipt_path.write_text(json.dumps(foreign_receipt), encoding="utf-8")
        snapshots_before_refusal = profile_bytes(foreign_data / "snapshots")
        invalid_backup = foreign_command("--snapshot-profile")
        assert invalid_backup.returncode == 64 and invalid_backup.stderr.strip() == "E_ACTIVE_PROFILE", invalid_backup
        assert profile_bytes(foreign_data / "snapshots") == snapshots_before_refusal
        print("PASS: engine-mismatched profile permits byte-preserving backup only; runtime and inconsistent pointer remain refused without API")

        foreign_receipt_path.write_bytes(valid_foreign_receipt)
        rollback_saved = profile_bytes(rollback_candidate / "profile")
        rollback_snapshots_before = profile_bytes(foreign_data / "snapshots")
        rollback_frozen_before = profile_bytes(foreign_data / "rollback-verified")
        changed_active = foreign_candidate / "profile" / "after-rollback-verification.txt"
        changed_active.write_text("preserve newly written data", encoding="utf-8")
        refused = foreign_command("--activate-rollback", rollback_id)
        assert refused.returncode == 64 and refused.stderr.strip() == "E_ROLLBACK_CHANGED", refused
        assert changed_active.read_text(encoding="utf-8") == "preserve newly written data"
        assert foreign_pointer_path.read_bytes() == pointer_before and not requests
        changed_active.unlink()  # Restore this controlled fixture, not a product recovery policy.
        with deny_pointer_replacement(foreign_pointer_path):
            failed_rollback = foreign_command("--activate-rollback", rollback_id)
        assert failed_rollback.returncode == 64 and failed_rollback.stderr.strip() == "E_ACTIVATION_WRITE", failed_rollback
        assert foreign_pointer_path.read_bytes() == pointer_before
        rollback_pending = foreign_data / "active-profile.json.pending"
        rollback_pending_bytes = rollback_pending.read_bytes()
        assert json.loads(rollback_pending_bytes)["candidateId"] == rollback_id
        assert json.loads(rollback_pending_bytes)["profileKind"] == "rollback"
        assert profile_bytes(rollback_candidate / "profile") == rollback_saved
        assert profile_bytes(foreign_candidate / "profile") == foreign_before
        assert profile_bytes(foreign_data / "snapshots") == rollback_snapshots_before
        assert profile_bytes(foreign_data / "rollback-verified") == rollback_frozen_before
        retry = foreign_command("--activate-rollback", rollback_id)
        assert retry.returncode == 64 and retry.stderr.strip() == "E_ACTIVATION_PENDING", retry
        assert rollback_pending.read_bytes() == rollback_pending_bytes and not requests
        archived = foreign_command("--archive-activation-pending")
        assert archived.returncode == 0, archived
        archive_id = str(uuid.UUID(archived.stdout.strip()))
        assert (foreign_data / "activation-recovery" / archive_id / "pending.json").read_bytes() == rollback_pending_bytes
        assert foreign_pointer_path.read_bytes() == pointer_before
        assert not rollback_pending.exists() and not requests
        committed = foreign_command("--activate-rollback", rollback_id)
        assert committed.returncode == 0, committed
        rollback_pointer = json.loads(foreign_pointer_path.read_text(encoding="utf-8"))
        assert rollback_pointer["schema"] == 2 and rollback_pointer["profileKind"] == "rollback"
        assert rollback_pointer["candidateId"] == rollback_id
        assert rollback_pointer["verificationId"] == rollback_validation["verificationId"]
        assert rollback_pointer["sourceSnapshotId"] == all_snapshot_id
        assert rollback_pointer["preservationSnapshotId"] == preservation_id
        assert rollback_pointer["engine"] == plan["targetEngine"]
        assert rollback_pointer["adapter"] == "stream-json-v1"
        assert not (foreign_data / "active-profile.json.pending").exists()
        assert profile_bytes(rollback_candidate / "profile") == rollback_saved
        assert profile_bytes(foreign_candidate / "profile") == foreign_before
        assert profile_bytes(foreign_data / "snapshots" / preservation_id / "profile") == foreign_before
        assert profile_bytes(old_source) == old_source_before and not requests
        for workspace, session_id in workspace_sessions.items():
            restarted = subprocess.run([str(executable), "--sessions"], cwd=workspace,
                env=foreign_env, capture_output=True, text=True, encoding="utf-8", timeout=60)
            assert restarted.returncode == 0, restarted
            assert session_id in restarted.stdout, restarted
        assert not requests
        print("PASS: public rollback activation selects verified history on restart without API and preserves newer active data")

        committed_pointer_bytes = foreign_pointer_path.read_bytes()
        repeated_rollback = foreign_command("--activate-rollback", rollback_id)
        assert repeated_rollback.returncode == 64 and repeated_rollback.stderr.strip() == "E_ACTIVATION_ALREADY_ACTIVE", repeated_rollback
        assert foreign_pointer_path.read_bytes() == committed_pointer_bytes and not requests
        for (workspace, session_id), marker in zip(workspace_sessions.items(), history_markers):
            resumed = subprocess.run([str(executable), "--resume", session_id, "--print", "after verified rollback"],
                cwd=workspace, env=rollback_env, capture_output=True, text=True, encoding="utf-8", timeout=60)
            assert resumed.returncode == 0, resumed
            assert marker in json.dumps(requests[-1].get("messages", [])[:-1]), "Rollback lost original session history"
        assert len(requests) == len(workspace_sessions)
        assert profile_bytes(rollback_candidate / "profile") != rollback_saved
        assert profile_bytes(foreign_candidate / "profile") == foreign_before
        assert profile_bytes(foreign_data / "snapshots") == rollback_snapshots_before
        assert profile_bytes(foreign_data / "rollback-verified") == rollback_frozen_before
        assert foreign_pointer_path.read_bytes() == committed_pointer_bytes
        print("PASS: rollback rejects newer writes, preserves Windows replacement failure evidence, archives before retry, and resumes both real sessions")

        if previous_executable is not None:
            # Actual two-version lifecycle: no rewritten engine metadata or receipts.
            cross_data = executable.parent / "cross version data"
            cross_env = dict(env, CCODE_DATA_DIR=str(cross_data))
            cross_offline = {key: value for key, value in cross_env.items()
                             if not key.startswith(("A_", "ANTHROPIC_", "CLAUDE_"))}
            def cross_command(binary, *arguments, offline=False):
                result = subprocess.run([str(binary), *arguments], cwd=executable.parent,
                    env=cross_offline if offline else cross_env, capture_output=True,
                    text=True, encoding="utf-8", timeout=120)
                assert result.returncode == 0, result
                return result
            assert "engine 2.1.221" in cross_command(previous_executable, "--version", offline=True).stdout
            assert "engine 2.1.282" in cross_command(executable, "--version", offline=True).stdout
            cross_command(previous_executable, "--print", history_markers[0])
            old_listing = cross_command(previous_executable, "--sessions", offline=True)
            old_match = re.search(r"(?m)^\d+\. ([0-9a-f-]{36})  ", old_listing.stdout)
            assert old_match, old_listing
            cross_session = str(uuid.UUID(old_match[1]))
            source_id = str(uuid.UUID(cross_command(previous_executable, "--snapshot-profile", offline=True).stdout.strip()))
            cross_source = cross_data / "snapshots" / source_id
            cross_source_bytes = profile_bytes(cross_source)
            legacy_bytes = profile_bytes(cross_data / "profile")
            next_candidate = str(uuid.UUID(cross_command(executable, "--stage-profile", source_id, offline=True).stdout.strip()))
            requests.clear()
            cross_command(executable, "--validate-profile", next_candidate, "--all-sessions")
            assert len(requests) == 1 and history_markers[0] in json.dumps(requests[0].get("messages", [])[:-1])
            requests.clear()
            cross_command(executable, "--activate-profile", next_candidate, offline=True)
            assert not requests
            next_pointer = json.loads((cross_data / "active-profile.json").read_text(encoding="utf-8"))
            assert next_pointer["engine"]["version"] == "2.1.282"
            newer_marker = "newer-engine-turn-preserved-4982"
            cross_command(executable, "--resume", cross_session, "--print", newer_marker)
            assert history_markers[0] in json.dumps(requests[-1].get("messages", [])[:-1])
            next_profile = cross_data / "candidates" / next_candidate / "profile"
            newer_bytes = profile_bytes(next_profile)
            assert any(newer_marker.encode() in content for content in newer_bytes.values())
            assert newer_bytes != legacy_bytes
            requests.clear()
            cross_plan = json.loads(cross_command(previous_executable, "--prepare-rollback", source_id, offline=True).stdout)
            assert cross_plan["targetEngine"]["version"] == "2.1.221"
            assert cross_plan["priorActivePointer"] == next_pointer
            assert cross_plan["targetEngine"]["sha256"] != next_pointer["engine"]["sha256"]
            cross_id = cross_plan["candidateId"]
            cross_preserved = cross_data / "snapshots" / cross_plan["preservationSnapshotId"] / "profile"
            assert profile_bytes(cross_preserved) == newer_bytes and not requests
            cross_command(previous_executable, "--validate-rollback", cross_id)
            assert len(requests) == 1
            assert history_markers[0] in json.dumps(requests[0].get("messages", [])[:-1])
            assert newer_marker not in json.dumps(requests[0].get("messages", []))
            requests.clear()
            cross_command(previous_executable, "--activate-rollback", cross_id, offline=True)
            assert not requests
            final_pointer = json.loads((cross_data / "active-profile.json").read_text(encoding="utf-8"))
            assert final_pointer["engine"] == cross_plan["targetEngine"] and final_pointer["profileKind"] == "rollback"
            cross_command(previous_executable, "--resume", cross_session, "--print", "after actual version rollback")
            assert len(requests) == 1
            assert history_markers[0] in json.dumps(requests[0].get("messages", [])[:-1])
            assert newer_marker not in json.dumps(requests[0].get("messages", []))
            assert profile_bytes(next_profile) == newer_bytes
            assert profile_bytes(cross_preserved) == newer_bytes
            assert profile_bytes(cross_data / "profile") == legacy_bytes
            assert profile_bytes(cross_source) == cross_source_bytes
            print("PASS: actual engines 2.1.221 -> 2.1.282 -> 2.1.221 preserve original history and newer data through verified rollback")

            requests.clear()
            active_after_rollback = cross_data / "rollback-candidates" / cross_id / "profile"
            active_before_refusal = profile_bytes(active_after_rollback)
            incompatible = subprocess.run([str(executable), "--sessions"], cwd=executable.parent,
                env=cross_offline, capture_output=True, text=True, encoding="utf-8", timeout=60)
            assert incompatible.returncode == 64 and incompatible.stderr.strip() == "E_ACTIVE_PROFILE", incompatible
            assert profile_bytes(active_after_rollback) == active_before_refusal and not requests
            identity_before_move = cross_command(previous_executable, "--workspace-id", offline=True).stdout.strip()
            assert str(uuid.UUID(identity_before_move)) == identity_before_move
            relocated_directory = executable.parent / "搬移 portable program"
            # Relocate the complete program, including its immutable extracted runtime.
            # Moving only the exe would legitimately materialize runtime on first use.
            original_program_directory = previous_executable.parent
            program_before = profile_bytes(original_program_directory)
            assert "ccode.exe" in program_before
            engine_name = "runtime/" + cross_plan["targetEngine"]["sha256"] + "/engine.exe"
            assert engine_name in program_before
            assert hashlib.sha256(program_before[engine_name]).hexdigest() == cross_plan["targetEngine"]["sha256"]
            shutil.move(str(original_program_directory), relocated_directory)
            relocated_binary = relocated_directory / "ccode.exe"
            assert not original_program_directory.exists()
            assert profile_bytes(relocated_directory) == program_before
            identity_after_move = cross_command(relocated_binary, "--workspace-id", offline=True).stdout.strip()
            assert identity_after_move == identity_before_move
            assert cross_session in cross_command(relocated_binary, "--sessions", offline=True).stdout
            assert not requests
            cross_command(relocated_binary, "--resume", cross_session, "--print", "after program directory relocation")
            assert len(requests) == 1
            assert history_markers[0] in json.dumps(requests[0].get("messages", [])[:-1])
            assert "after actual version rollback" in json.dumps(requests[0].get("messages", [])[:-1])
            assert newer_marker not in json.dumps(requests[0].get("messages", []))
            assert profile_bytes(relocated_directory) == program_before, "Program move must not redirect data into delivery files"
            assert profile_bytes(next_profile) == newer_bytes
            assert profile_bytes(cross_preserved) == newer_bytes
            assert profile_bytes(cross_data / "profile") == legacy_bytes
            assert profile_bytes(cross_source) == cross_source_bytes
            assert json.loads((cross_data / "active-profile.json").read_text(encoding="utf-8")) == final_pointer
            print("PASS: incompatible actual engine refuses selected profile; moved program preserves workspace identity, resumed history, and separate data")

            # Move the entire external data root without editing native history or receipts.
            requests.clear()
            data_before_move = profile_bytes(cross_data)
            old_data = cross_data
            cross_data = executable.parent / "搬移 external data"
            shutil.move(str(old_data), cross_data)
            assert not old_data.exists()
            assert profile_bytes(cross_data) == data_before_move
            cross_env["CCODE_DATA_DIR"] = str(cross_data)
            cross_offline["CCODE_DATA_DIR"] = str(cross_data)
            assert cross_command(relocated_binary, "--workspace-id", offline=True).stdout.strip() == identity_before_move
            assert cross_session in cross_command(relocated_binary, "--sessions", offline=True).stdout
            assert not requests
            cross_command(relocated_binary, "--resume", cross_session, "--print", "after external data relocation")
            assert len(requests) == 1
            resumed_history = json.dumps(requests[0].get("messages", [])[:-1])
            assert history_markers[0] in resumed_history
            assert "after actual version rollback" in resumed_history
            assert "after program directory relocation" in resumed_history
            assert newer_marker not in resumed_history
            assert not old_data.exists(), "Relocation must not recreate the old data root"
            assert profile_bytes(relocated_directory) == program_before
            for preserved in (next_profile, cross_preserved, old_data / "profile", cross_source):
                relative = preserved.relative_to(old_data)
                expected = {name[len(relative.as_posix()) + 1:]: content
                    for name, content in data_before_move.items()
                    if name.startswith(relative.as_posix() + "/")}
                assert profile_bytes(cross_data / relative) == expected
            assert json.loads((cross_data / "active-profile.json").read_text(encoding="utf-8")) == final_pointer
            print("PASS: relocated external data root preserves workspace identity, actual resumed history, backups and program separation")









    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    executable = Path(sys.argv[2]).resolve()
    if sys.argv[1] == "verify" and len(sys.argv) == 4:
        verify(executable, Path(sys.argv[3]).resolve())
    else:
        {"prepare": prepare, "verify": verify}[sys.argv[1]](executable)
