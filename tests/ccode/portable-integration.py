"""Public Windows launcher contract. No live credentials or model required."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import uuid
import shutil

def check(executable):
    with tempfile.TemporaryDirectory(prefix="ccode portable ") as folder:
        root = Path(folder)
        app = root / "ccode.exe"
        shutil.copy2(executable, app)
        env = {k: v for k, v in os.environ.items()
               if not k.startswith(("A_", "C_", "ANTHROPIC_", "CLAUDE_"))}
        def run(*args):
            return subprocess.run([str(app), *args], env=env, cwd=root,
                                  text=True, encoding="utf-8", capture_output=True, timeout=30)
        result = run("--version")
        assert result.returncode == 0, result.stderr
        assert result.stdout.startswith("ccode "), result.stdout
        result = run("--help")
        assert result.returncode == 0, result.stderr
        assert "--data-dir" in result.stdout and "--sessions" in result.stdout
        assert not any(name in result.stdout.lower() for name in ("claude", "anthropic"))
        assert not (root / "data").exists(), "Informational commands must not create profile data"
        test_id = "a2345678-1234-1234-1234-123456789abc"
        for args in (("--validate-rollback",),
                     ("--validate-rollback", test_id, "--sessions"),
                     ("--validate-rollback", test_id, "--resume", test_id),
                     ("--validate-rollback", test_id, "--all-sessions"),
                     ("--validate-rollback", test_id, "--prepare-rollback", test_id),
                     ("--validate-rollback", test_id, "--validate-profile", test_id, "--all-sessions"),
                     ("--validate-rollback", test_id, "--activate-profile", test_id),
                     ("--validate-rollback", test_id, "--snapshot-profile"),
                     ("--validate-rollback", test_id, "--archive-activation-pending"),
                     ("--validate-rollback", test_id, "--tools", "Bash"),
                     ("--validate-rollback", test_id, "unexpected prompt"),
                     ("--prepare-rollback", test_id, "--sessions"),
                     ("--prepare-rollback", test_id, "--snapshot-profile"),
                     ("--prepare-rollback", test_id, "--archive-activation-pending"),
                     ("--prepare-rollback", test_id, "--model", "test"),
                     ("--prepare-rollback", test_id, "unexpected prompt"),
                     ("--prepare-rollback", test_id, "--activate-profile", test_id),
                     ("--prepare-rollback", test_id, "--stage-profile", test_id),
                     ("--prepare-rollback", test_id, "--validate-profile", test_id, "--all-sessions"),
                     ("--prepare-rollback",),
                     ("--snapshot-profile", "--print"),
                     ("--snapshot-profile", "--stage-profile", test_id),
                     ("--snapshot-profile", "--model", "test"),
                     ("--snapshot-profile", "unexpected prompt"),
                     ("--archive-activation-pending", "--sessions"),
                     ("--archive-activation-pending", "--activate-profile", test_id),
                     ("--archive-activation-pending", "--model", "test"),
                     ("--archive-activation-pending", "unexpected prompt"),
                     ("--activate-profile", test_id, "--sessions"),
                     ("--activate-profile", test_id, "--model", "test"),
                     ("--activate-profile", test_id, "--snapshot-profile"),
                     ("--all-sessions",),
                     ("--validate-profile", test_id),
                     ("--validate-profile", test_id, "--all-sessions", "--resume", test_id),
                     ("--validate-profile", test_id, "--all-sessions", "--print")):
            invalid = run(*args)
            assert invalid.returncode == 64 and invalid.stderr.strip() == "E_ARGUMENT", invalid
        assert not (root / "data").exists(), "Invalid validation arguments must not create data"
        recovery_data = root / "recovery data"
        recovery_data.mkdir()
        pointer = recovery_data / "active-profile.json"
        pointer.write_bytes(b"malformed committed pointer")
        pending = recovery_data / "active-profile.json.pending"
        pending.write_bytes(b'{"schema":1,\x00truncated')
        result = run("--data-dir", str(recovery_data), "--archive-activation-pending")
        assert result.returncode == 0, result
        recovery_id = str(uuid.UUID(result.stdout.strip()))
        archived = recovery_data / "activation-recovery" / recovery_id / "pending.json"
        assert archived.read_bytes() == b'{"schema":1,\x00truncated'
        assert pointer.read_bytes() == b"malformed committed pointer"
        assert not pending.exists() and not (recovery_data / "profile").exists()
        result = run("--data-dir", str(recovery_data), "--archive-activation-pending")
        assert result.returncode == 64 and result.stderr.strip() == "E_ACTIVATION_PENDING_MISSING", result
        assert list((recovery_data / "activation-recovery").iterdir()) == [archived.parent]
        print("PASS: explicit pending archive preserves raw bytes and invalid committed pointer without creating a profile")
        env.update(A_AUTH_TOKEN="fake-token", A_BASE_URL="http://127.0.0.1:1")
        result = run("--sessions")
        assert result.returncode == 0, result.stderr
        assert "No saved sessions" in result.stdout
        # User paths and prompt contents are not product names.
        workspace = root / "claude-anthropic user files"
        workspace.mkdir()
        result = subprocess.run([str(app), "--sessions"], env=env, cwd=workspace,
                                text=True, capture_output=True, timeout=30)
        assert result.returncode == 0, result.stderr
        assert not list(root.rglob("cc-runtime.dll")), "New launcher must not extract an injection DLL"
        # A missing/invalid frontend console must never fall back to a hidden
        # worker console, hang for approval, or silently authorize the tool.
        request = {"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {
            "name": "approve", "arguments": {"tool_name": "Write", "input": {
                "file_path": "unapproved.txt", "content": "must not write"}}}}
        for owner in ("", "0", "not-a-pid", "999999999999999999999", "4294967294"):
            worker_env = dict(env, CCODE_INTERACTIVE="1", CCODE_FRONTEND_PID=owner)
            result = subprocess.run([str(app), "--ccode-permission-server"], env=worker_env,
                                    cwd=root, input=json.dumps(request) + "\n", text=True,
                                    encoding="utf-8", capture_output=True, timeout=10)
            assert result.returncode == 0, (owner, result.stderr)
            response = json.loads(result.stdout)
            decision = json.loads(response["result"]["content"][0]["text"])
            assert response["id"] == 1 and decision["behavior"] == "deny", (owner, response)
        print("PASS: unavailable frontend console fails closed without a hidden approval prompt")
        broken_selection = root / "broken selection"
        broken_selection.mkdir()
        (broken_selection / "active-profile.json").write_text("{broken", encoding="utf-8")
        rejected_selection = run("--data-dir", str(broken_selection), "--sessions")
        assert rejected_selection.returncode == 64 and rejected_selection.stderr.strip() == "E_ACTIVE_PROFILE", rejected_selection
        assert not (broken_selection / "profile").exists(), "Invalid active pointer must not create a fallback profile"
        assert (broken_selection / "active-profile.json").read_text(encoding="utf-8") == "{broken"
        print("PASS: invalid active-profile pointer fails closed without fallback or overwriting state")
        data = root / "external data"
        identity = run("--data-dir", str(data), "--workspace-id")
        assert identity.returncode == 0, identity.stderr
        workspace_id = str(uuid.UUID(identity.stdout.strip()))
        relocated = root / "relocated app"
        relocated.mkdir()
        shutil.copy2(app, relocated / "ccode.exe")
        restarted = subprocess.run([str(relocated / "ccode.exe"), "--data-dir", str(data), "--workspace-id"],
                                   env=env, cwd=root, text=True, capture_output=True, timeout=15)
        assert restarted.returncode == 0 and restarted.stdout.strip() == workspace_id, restarted
        different = subprocess.run([str(app), "--data-dir", str(data), "--workspace-id"],
                                   env=env, cwd=workspace, text=True, capture_output=True, timeout=15)
        assert different.returncode == 0 and str(uuid.UUID(different.stdout.strip())) != workspace_id
        registry = data / "profile" / "workspaces.json"
        committed = registry.read_bytes()
        pending = registry.with_name(registry.name + ".new")
        pending_bytes = b'{interrupted-identity-private-marker'
        pending.write_bytes(pending_bytes)
        existing = run("--data-dir", str(data), "--workspace-id")
        assert existing.returncode == 0 and existing.stdout.strip() == workspace_id
        new_workspace = root / "new workspace"
        new_workspace.mkdir()
        def register_new_workspace():
            return subprocess.run([str(app), "--data-dir", str(data), "--workspace-id"],
                                  env=env, cwd=new_workspace, text=True, encoding="utf-8",
                                  capture_output=True, timeout=15)
        blocked = register_new_workspace()
        assert blocked.returncode == 64 and blocked.stderr.strip() == "E_WORKSPACE_PENDING"
        assert not blocked.stdout and "private-marker" not in blocked.stderr
        assert pending.read_bytes() == pending_bytes and registry.read_bytes() == committed
        archive = data / "preserved-workspace-update"
        pending.rename(archive)
        retried = register_new_workspace()
        assert retried.returncode == 0 and str(uuid.UUID(retried.stdout.strip())) != workspace_id
        assert archive.read_bytes() == pending_bytes and not pending.exists()
        assert run("--data-dir", str(data), "--workspace-id").stdout.strip() == workspace_id
        print("PASS: interrupted workspace registry update is preserved; committed identity remains readable and explicit archive permits retry")
        registry.write_text('{"schema":999,"workspaces":{}}')
        rejected = run("--data-dir", str(data), "--workspace-id")
        assert rejected.returncode != 0 and "E_WORKSPACE_DATA" in rejected.stderr, rejected
        assert registry.read_text() == '{"schema":999,"workspaces":{}}'
        print("PASS: persistent workspace UUID survives frontend relocation; corrupt registry fails without replacement")
        history_data = root / "history data"
        history = history_data / "profile" / "home" / ".claude" / "projects" / "fixture"
        history.mkdir(parents=True)
        session_id = str(uuid.uuid4())
        transcript = history / (session_id + ".jsonl")
        for field in ("type", "sessionId", "cwd", "isSidechain"):
            event = {"type": "user", "sessionId": session_id, "cwd": str(root),
                     "isSidechain": False, "message": {"content": "private-history-marker"}}
            event[field] = 42
            saved = (json.dumps(event) + "\n").encode("utf-8")
            transcript.write_bytes(saved)
            result = run("--data-dir", str(history_data), "--sessions")
            assert result.returncode == 64 and result.stderr.strip() == "E_SESSION_DATA", result
            assert "private-history-marker" not in result.stdout + result.stderr
            assert transcript.read_bytes() == saved
        print("PASS: corrupt history metadata is classified without disclosure or transcript changes")
        valid_event = {"type": "user", "sessionId": session_id, "cwd": str(root),
                       "isSidechain": False, "message": {"content": "private-history-marker"}}
        for tail in (b'{truncated-private-marker', b'[]\n', b'{broken}\n'):
            saved = (json.dumps(valid_event) + "\n").encode("utf-8") + tail
            transcript.write_bytes(saved)
            result = run("--data-dir", str(history_data), "--sessions")
            assert result.returncode == 0 and not result.stderr, result
            assert session_id in result.stdout and "[unavailable: E_SESSION_DATA]" in result.stdout
            assert "private-history-marker" not in result.stdout + result.stderr
            assert "truncated-private-marker" not in result.stdout + result.stderr
            for resume in (("--resume", session_id), ("--continue",)):
                rejected = run("--data-dir", str(history_data), *resume, "--print", "No replay")
                assert rejected.returncode == 64 and rejected.stderr.strip() == "E_SESSION_DATA", rejected
            assert transcript.read_bytes() == saved
        print("PASS: malformed history tails remain visible as unavailable and cannot resume; original bytes preserved")
        # Backup works without loading or repairing malformed history. The live
        # profile lock is excluded; all other bytes are independently hashed.
        profile = history_data / "profile"
        before = {path.relative_to(profile).as_posix(): path.read_bytes()
                  for path in profile.rglob("*") if path.is_file() and path.name != "frontend.lock"}
        backup = run("--data-dir", str(history_data), "--snapshot-profile")
        assert backup.returncode == 0, backup.stderr
        backup_id = str(uuid.UUID(backup.stdout.strip()))
        snapshot = history_data / "snapshots" / backup_id
        manifest = json.loads((snapshot / "manifest.json").read_text(encoding="utf-8"))
        assert manifest["schema"] == 1 and manifest["snapshotId"] == backup_id
        assert set(manifest["files"]) == set(before)
        for name, saved in before.items():
            assert (profile / name).read_bytes() == saved
            assert (snapshot / "profile" / name).read_bytes() == saved
            assert manifest["files"][name] == {"sha256": hashlib.sha256(saved).hexdigest(), "size": len(saved)}
        assert not (snapshot / "profile/frontend.lock").exists()
        assert not (history_data / "snapshots" / (backup_id + ".pending")).exists()
        print("PASS: locked profile snapshot preserves source bytes and independently verified SHA256 manifest")
        staged = run("--data-dir", str(history_data), "--stage-profile", backup_id)
        assert staged.returncode == 0, staged.stderr
        candidate_id = str(uuid.UUID(staged.stdout.strip()))
        candidate = history_data / "candidates" / candidate_id
        metadata = json.loads((candidate / "candidate.json").read_text(encoding="utf-8"))
        assert metadata == {"schema": 1, "candidateId": candidate_id,
                            "sourceSnapshotId": backup_id, "state": "staged"}
        for name, saved in before.items():
            assert (candidate / "profile" / name).read_bytes() == saved
            assert (snapshot / "profile" / name).read_bytes() == saved
            assert (profile / name).read_bytes() == saved
        assert not (history_data / "active-profile.json").exists()
        candidates_before = set((history_data / "candidates").iterdir())
        manifest_bytes = (snapshot / "manifest.json").read_bytes()
        name = next(iter(before))
        victim = snapshot / "profile" / name
        saved = victim.read_bytes()
        for damage in ("tamper", "missing", "extra", "traversal"):
            if damage == "tamper":
                victim.write_bytes(b"corrupted")
            elif damage == "missing":
                victim.unlink()
            elif damage == "extra":
                (snapshot / "profile/unexpected.bin").write_bytes(b"extra")
            else:
                invalid = json.loads(manifest_bytes)
                invalid["files"]["../outside"] = {"sha256": "0" * 64, "size": 0}
                (snapshot / "manifest.json").write_text(json.dumps(invalid), encoding="utf-8")
            rejected = run("--data-dir", str(history_data), "--stage-profile", backup_id)
            expected = "E_SNAPSHOT_DATA" if damage == "traversal" else "E_SNAPSHOT_INTEGRITY"
            assert rejected.returncode == 64 and rejected.stderr.strip() == expected, (damage, rejected)
            assert set((history_data / "candidates").iterdir()) == candidates_before
            victim.write_bytes(saved)
            (snapshot / "profile/unexpected.bin").unlink(missing_ok=True)
            (snapshot / "manifest.json").write_bytes(manifest_bytes)
        assert all((profile / name).read_bytes() == saved for name, saved in before.items())
        print("PASS: isolated candidate matches verified snapshot; tampered, missing, extra and traversal data rejected")
        print("portable frontend integration passed")

if __name__ == "__main__":
    check(Path(sys.argv[1]).resolve())
