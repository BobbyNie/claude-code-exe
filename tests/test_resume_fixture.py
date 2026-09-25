"""Guard the fake API oracle against observed native message envelopes."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "resume_integration", Path(__file__).parent / "ccode/resume-integration.py")
resume = importlib.util.module_from_spec(spec)
spec.loader.exec_module(resume)


class PreparationFixtureTests(unittest.TestCase):
    def test_standalone_preparation_includes_invalid_history_for_preflight(self):
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "ccode.exe"
            resume.prepare(executable)
            legacy = Path(directory) / "data/cc/profile/home/.cc/projects"
            invalid = list(legacy.rglob("invalid-session.jsonl"))
            self.assertEqual(len(invalid), 1)
            self.assertEqual(invalid[0].read_text(encoding="utf-8"), "invalid-history-fixture\n")


class ResumeFixtureTests(unittest.TestCase):
    markers = ["legacy-resume-marker-7391", "second-workspace-marker-6842"]
    probe = "For profile recovery verification, repeat the first user message."

    def messages(self):
        return [{"role": "user", "content": self.markers[0]},
                {"role": "assistant", "content": "recorded"},
                {"role": "user", "content": self.probe}]

    def test_plain_probe(self):
        self.assertEqual(resume.fixture_answer(self.messages(), self.markers), self.markers[0])

    def test_native_trailing_system_message(self):
        messages = self.messages() + [{"role": "system", "content": [{"type": "text", "text": "context"}]}]
        self.assertEqual(resume.fixture_answer(messages, self.markers), self.markers[0])

    def test_wrong_answer_mode(self):
        self.assertEqual(resume.fixture_answer(self.messages(), self.markers, False), "history-verification-failed")

    def test_answer_leak_is_rejected(self):
        messages = self.messages()
        messages[-1]["content"] += self.markers[0]
        with self.assertRaises(AssertionError):
            resume.fixture_answer(messages, self.markers)

    def test_ambiguous_history_is_not_verified(self):
        messages = self.messages()
        messages[0]["content"] += self.markers[1]
        self.assertEqual(resume.fixture_answer(messages, self.markers), "history-verification-failed")

    def test_old_probe_does_not_classify_new_user_turn(self):
        messages = self.messages() + [{"role": "user", "content": "new ordinary turn"}]
        self.assertEqual(resume.fixture_answer(messages, self.markers), "resume-test-ok")




class PointerFaultFixtureTests(unittest.TestCase):
    def test_replacement_lock_is_released_even_on_test_failure(self):
        import ctypes
        from unittest.mock import Mock, patch
        kernel = Mock()
        kernel.CreateFileW.return_value = 123
        kernel.CloseHandle.return_value = 1
        with patch.object(ctypes, "WinDLL", return_value=kernel, create=True):
            with self.assertRaisesRegex(RuntimeError, "test failure"):
                with resume.deny_pointer_replacement(Path("active-profile.json")):
                    kernel.CloseHandle.assert_not_called()
                    raise RuntimeError("test failure")
        kernel.CreateFileW.assert_called_once_with("active-profile.json", 0x80000000, 3, None, 3, 0, None)
        kernel.CloseHandle.assert_called_once_with(123)

    def test_failed_lock_acquisition_does_not_run_fault_case(self):
        import ctypes
        from unittest.mock import Mock, patch
        kernel = Mock()
        kernel.CreateFileW.return_value = ctypes.c_void_p(-1).value
        with patch.object(ctypes, "WinDLL", return_value=kernel, create=True):
            with self.assertRaisesRegex(AssertionError, "replacement lock"):
                with resume.deny_pointer_replacement(Path("active-profile.json")):
                    self.fail("Fault precondition was not established")
        kernel.CloseHandle.assert_not_called()


if __name__ == "__main__":
    unittest.main()
