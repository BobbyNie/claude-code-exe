"""Guard the fake API oracle against observed native message envelopes."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "resume_integration", Path(__file__).parent / "ccode/resume-integration.py")
resume = importlib.util.module_from_spec(spec)
spec.loader.exec_module(resume)


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


if __name__ == "__main__":
    unittest.main()
