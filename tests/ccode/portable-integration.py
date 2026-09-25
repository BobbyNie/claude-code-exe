"""Public Windows launcher contract. No live credentials or model required."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
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
        print("portable frontend integration passed")

if __name__ == "__main__":
    check(Path(sys.argv[1]).resolve())
