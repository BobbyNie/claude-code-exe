"""Public Windows launcher contract. No live credentials or model required."""
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
        print("portable frontend integration passed")

if __name__ == "__main__":
    check(Path(sys.argv[1]).resolve())
