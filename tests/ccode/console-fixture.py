"""Drive the real Windows console permission UI; never bypass the permission adapter."""
import ctypes
from ctypes import wintypes as w
import signal
import json
import subprocess
import threading
import time


def process_diagnostics(pid):
    """Bounded, numeric-only process/thread snapshot; never replace the primary failure."""
    def unavailable(reason):
        return {"status": "unavailable", "reason": reason, "processes": []}
    if type(pid) is not int or pid <= 0:
        return unavailable("invalid_pid")
    script = r'''
$ErrorActionPreference = 'Stop'
$rootId = ROOT_PID
$all = @(Get-CimInstance Win32_Process | Select-Object ProcessId,ParentProcessId)
$ids = @($rootId)
do {
    $added = @($all | Where-Object { $_.ParentProcessId -in $ids -and $_.ProcessId -notin $ids } | ForEach-Object { [int]$_.ProcessId })
    $ids += $added
} while ($added.Count -gt 0)
$rows = @($all | Where-Object { $_.ProcessId -in $ids } | ForEach-Object {
    $row = $_
    $threads = @()
    try {
        $threads = @((Get-Process -Id $row.ProcessId -ErrorAction Stop).Threads | ForEach-Object {
            $state = [int]$_.ThreadState
            $reason = -1
            if ($state -eq 5) { try { $reason = [int]$_.WaitReason } catch {} }
            @{id=[int]$_.Id; state=$state; wait=$reason}
        })
    } catch {}
    @{pid=[int]$row.ProcessId; parent=[int]$row.ParentProcessId; threads=$threads}
})
ConvertTo-Json -InputObject $rows -Depth 5 -Compress
'''.replace("ROOT_PID", str(pid))
    try:
        result = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
                                stdin=subprocess.DEVNULL, capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=10,
                                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        if result.returncode != 0:
            return unavailable("collector_exit")
        rows = json.loads(result.stdout)
        if not isinstance(rows, list):
            return unavailable("invalid_output")
        processes = []
        def number(value):
            if type(value) is not int:
                raise ValueError("Non-numeric diagnostic field")
            return value
        for row in rows:
            processes.append({"pid": number(row["pid"]), "parent": number(row["parent"]),
                              "threads": [{key: number(thread[key]) for key in ("id", "state", "wait")}
                                          for thread in row["threads"]]})
        return {"status": "captured", "processes": processes}
    except subprocess.TimeoutExpired:
        return unavailable("timeout")
    except OSError:
        return unavailable("launch_failed")
    except subprocess.SubprocessError:
        return unavailable("collector_failed")
    except (ValueError, TypeError, KeyError):
        return unavailable("invalid_output")


def wait_for_persisted_history(data, process, timeout=15):
    """Establish recovery evidence before killing an asynchronously writing engine."""
    deadline = time.monotonic() + timeout
    while process.poll() is None and time.monotonic() < deadline:
        for path in data.rglob("*.jsonl"):
            try:
                lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
            except (OSError, UnicodeError):
                continue
            for line in lines:
                if not line.endswith("\n"):
                    continue  # A partial write is not persisted transcript evidence.
                try:
                    record = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if isinstance(record, dict) and record.get("type") == "user":
                    message = record.get("message")
                    if isinstance(message, dict) and message.get("role") == "user" and message.get("content"):
                        return
        time.sleep(0.05)
    raise AssertionError("No persisted user history before cancellation; recovery precondition unmet")


def check_permission(app, workspace, data, env, mode, target):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)

    class Coord(ctypes.Structure):
        _fields_ = [("X", w.SHORT), ("Y", w.SHORT)]

    class Rect(ctypes.Structure):
        _fields_ = [("Left", w.SHORT), ("Top", w.SHORT), ("Right", w.SHORT), ("Bottom", w.SHORT)]

    class Info(ctypes.Structure):
        _fields_ = [("size", Coord), ("cursor", Coord), ("attributes", w.WORD),
                    ("window", Rect), ("maximum", Coord)]

    class Key(ctypes.Structure):
        _fields_ = [("down", w.BOOL), ("repeat", w.WORD), ("virtual", w.WORD),
                    ("scan", w.WORD), ("char", w.WCHAR), ("control", w.DWORD)]

    class Event(ctypes.Union):
        _fields_ = [("key", Key), ("padding", ctypes.c_byte * 16)]

    class Record(ctypes.Structure):
        _fields_ = [("kind", w.WORD), ("event", Event)]

    kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, ctypes.c_void_p,
                                  w.DWORD, w.DWORD, w.HANDLE]
    kernel.CreateFileW.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.GetConsoleScreenBufferInfo.argtypes = [w.HANDLE, ctypes.POINTER(Info)]
    kernel.ReadConsoleOutputCharacterW.argtypes = [w.HANDLE, w.LPWSTR, w.DWORD, Coord,
                                                  ctypes.POINTER(w.DWORD)]
    kernel.WriteConsoleInputW.argtypes = [w.HANDLE, ctypes.POINTER(Record), w.DWORD,
                                         ctypes.POINTER(w.DWORD)]
    kernel.FlushConsoleInputBuffer.argtypes = [w.HANDLE]
    allocated = not kernel.GetConsoleCP()
    if allocated:
        assert kernel.AllocConsole(), "Cannot allocate interactive acceptance console"
    handles = []
    process = None
    try:
        for name in ("CONIN$", "CONOUT$"):
            handle = kernel.CreateFileW(name, 0xC0000000, 3, None, 3, 0, None)
            assert handle != ctypes.c_void_p(-1).value, "Cannot open test console"
            handles.append(handle)
        input_handle, output_handle = handles
        assert kernel.FlushConsoleInputBuffer(input_handle)

        def screen():
            info = Info()
            assert kernel.GetConsoleScreenBufferInfo(output_handle, ctypes.byref(info))
            length = info.size.X * info.size.Y
            text = ctypes.create_unicode_buffer(length + 1)
            read = w.DWORD()
            assert kernel.ReadConsoleOutputCharacterW(output_handle, text, length, Coord(0, 0), ctypes.byref(read))
            return text[:read.value]

        def type_line(text):
            records = (Record * (len(text) * 2))()
            for i, char in enumerate(text):
                for up in (0, 1):
                    records[2 * i + up].kind = 1
                    records[2 * i + up].event.key = Key(not up, 1, 13 if char == "\r" else 0, 0, char, 0)
            written = w.DWORD()
            assert kernel.WriteConsoleInputW(input_handle, records, len(records), ctypes.byref(written))
            assert written.value == len(records)

        # A unique file name prevents a previous console prompt from satisfying this case.
        marker = target.name
        assert marker not in screen(), "Fixture target must be unique per console case"
        with open("CONIN$", "rb", buffering=0) as console_input:
            process = subprocess.Popen([str(app), "--data-dir", str(data),
                                        "Execute the requested write after asking permission."],
                                       cwd=workspace, env=env, stdin=console_input,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
        captured = [bytearray(), bytearray()]

        def drain(pipe, buffer):
            while byte := pipe.read(1):
                buffer.extend(byte)

        readers = [threading.Thread(target=drain, args=(pipe, buffer), daemon=True)
                   for pipe, buffer in zip((process.stdout, process.stderr), captured)]
        for reader in readers:
            reader.start()
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline and process.poll() is None:
            text = screen()
            if marker in text and "Type yes to allow once; Enter denies:" in text:
                break
            time.sleep(0.05)
        else:
            print("DIAGNOSTIC permission-prompt-timeout:", json.dumps(process_diagnostics(process.pid)), flush=True)
            raise AssertionError(("No real permission prompt", process.poll(), captured,
                                  "console", screen().rstrip(" \x00")[-8000:]))
        assert not target.exists(), "Write occurred before user approval"
        if mode == "cancel":
            wait_for_persisted_history(data, process)
            assert not target.exists(), "Write occurred while awaiting history persistence"
            process.send_signal(signal.CTRL_BREAK_EVENT)
            deadline = time.monotonic() + 15
            while b"Cancelled" not in captured[1] and process.poll() is None and time.monotonic() < deadline:
                time.sleep(0.05)
            assert b"Cancelled" in captured[1], captured
            type_line("/exit\r")
        else:
            type_line(("yes" if mode == "allow" else "no") + "\r/exit\r")
        process.wait(timeout=30)
        for reader in readers:
            reader.join(timeout=5)
            assert not reader.is_alive(), "Permission descendant kept output pipe open"
        assert process.returncode == (130 if mode == "cancel" else 0), (process.returncode, captured)
        if mode == "allow":
            assert target.read_text() == "approved-write\n"
        else:
            assert not target.exists(), "Unapproved tool changed the workspace"
        print(f"PASS: interactive permission {mode}; real console and real Write side effects")
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                print("DIAGNOSTIC permission-kill-timeout:", json.dumps(process_diagnostics(process.pid)), flush=True)
                raise
        for handle in handles:
            kernel.CloseHandle(handle)
        if allocated:
            kernel.FreeConsole()
