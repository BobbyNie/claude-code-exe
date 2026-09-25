"""Cancellation recovery must establish persisted history before interrupting."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location(
    "console_fixture", Path(__file__).parent / "ccode/console-fixture.py")
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class ConsoleFixtureTests(unittest.TestCase):
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
