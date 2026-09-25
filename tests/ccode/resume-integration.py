"""Exercise real --resume against a local fake API, without user credentials."""
import hashlib
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
    probe_reply_correct = True

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
            answer = "resume-test-ok"
            messages = body.get("messages", [])
            if messages and "For profile recovery verification" in json.dumps(messages[-1]):
                assert "legacy-resume-marker-7391" not in json.dumps(messages[-1]), "Probe leaked the expected answer"
                answer = "legacy-resume-marker-7391" if probe_reply_correct and \
                    "legacy-resume-marker-7391" in json.dumps(messages[:-1]) else "history-verification-failed"
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



    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    executable = Path(sys.argv[2]).resolve()
    {"prepare": prepare, "verify": verify}[sys.argv[1]](executable)
