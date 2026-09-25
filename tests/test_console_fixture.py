"""Cancellation recovery must establish persisted history before interrupting."""
import importlib.util
import json
import os
import subprocess
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location(
    "console_fixture", Path(__file__).parent / "ccode/console-fixture.py")
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class ConsoleFixtureTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "Requires actual Windows process/thread API")
    def test_windows_process_diagnostics_capture_actual_live_threads(self):
        snapshot = fixture.process_diagnostics(os.getpid())
        self.assertEqual(snapshot["status"], "captured")
        current = next(row for row in snapshot["processes"] if row["pid"] == os.getpid())
        self.assertTrue(current["threads"], "No actual thread state captured")

    def test_process_diagnostics_discard_non_whitelisted_content(self):
        result = Mock(returncode=0, stdout=json.dumps([
            {"pid": 123, "parent": 12, "threads": [
                {"id": 321, "state": 5, "wait": 4, "secret": "private prompt"}],
             "command": "secret token", "environment": "private credential"}]), stderr="secret stderr")
        with patch.object(fixture.subprocess, "run", return_value=result) as run:
            snapshot = fixture.process_diagnostics(123)
        self.assertEqual(snapshot, {"status": "captured", "processes": [
            {"pid": 123, "parent": 12, "threads": [{"id": 321, "state": 5, "wait": 4}]}]})
        self.assertEqual(run.call_args.kwargs["timeout"], 10)

    def test_process_diagnostic_errors_do_not_expose_output_or_mask_primary_failure(self):
        for outcome in (Mock(returncode=1, stdout="private", stderr="secret"),
                        Mock(returncode=0, stdout="not-json secret", stderr=""),
                        subprocess.TimeoutExpired("sensitive command", 10, output="secret")):
            with self.subTest(outcome=type(outcome).__name__):
                options = {"side_effect": outcome} if isinstance(outcome, Exception) else {"return_value": outcome}
                with patch.object(fixture.subprocess, "run", **options):
                    snapshot = fixture.process_diagnostics(123)
                self.assertEqual(snapshot, {"status": "unavailable", "processes": []})

    def test_waits_for_persisted_user_record_not_just_file_creation(self):
        with tempfile.TemporaryDirectory() as folder:
            data = Path(folder)
            transcript = data / "session.jsonl"
            transcript.touch()
            process = Mock()
            process.poll.return_value = None
            def flush_history(_):
                transcript.write_text('{"type":"user","message":{"role":"user","content":"fixture"}}\n')
            with patch.object(fixture.time, "sleep", side_effect=flush_history) as sleep:
                fixture.wait_for_persisted_history(data, process, timeout=1)
            sleep.assert_called_once()

    def test_partial_or_non_user_records_do_not_satisfy_precondition(self):
        for contents in ('{"type":"user","message":{"role":"user","content":"x"}}',
                         '{"type":"assistant"}\n', 'broken\n', '[]\n'):
            with self.subTest(contents=contents), tempfile.TemporaryDirectory() as folder:
                data = Path(folder)
                (data / "session.jsonl").write_text(contents)
                process = Mock()
                process.poll.return_value = None
                with patch.object(fixture.time, "monotonic", side_effect=[0, 0, 2]), \
                     patch.object(fixture.time, "sleep"), \
                     self.assertRaisesRegex(AssertionError, "persisted user history"):
                    fixture.wait_for_persisted_history(data, process, timeout=1)

    def test_exited_process_does_not_count_as_persisted_history(self):
        with tempfile.TemporaryDirectory() as folder:
            process = Mock()
            process.poll.return_value = 1
            with self.assertRaisesRegex(AssertionError, "persisted user history"):
                fixture.wait_for_persisted_history(Path(folder), process, timeout=1)


if __name__ == "__main__":
    unittest.main()
