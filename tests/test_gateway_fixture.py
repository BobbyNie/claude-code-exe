"""Validate that interrupted-stream fixtures exercise semantic EOF boundaries."""
import importlib.util
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "gateway_fixture", Path(__file__).parent / "ccode/gateway-integration.py")
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class GatewayFixtureTests(unittest.TestCase):
    def test_complete_arguments_still_lack_block_and_message_termination(self):
        events = fixture.unfinished_tool_events("fixture-model", "target.txt", "marker", True)
        self.assertEqual([kind for kind, _ in events],
                         ["message_start", "content_block_start", "content_block_delta"])
        start = events[1][1]["content_block"]
        self.assertEqual((start["type"], start["name"], start["input"]),
                         ("tool_use", "Write", {}))
        arguments = json.loads(events[2][1]["delta"]["partial_json"])
        self.assertEqual(arguments, {"file_path": "target.txt", "content": "marker"})
        self.assertIsNone(events[0][1]["message"]["stop_reason"])

    def test_existing_incomplete_argument_case_remains_invalid_json(self):
        events = fixture.unfinished_tool_events("fixture-model", "target.txt", "marker", False)
        self.assertEqual(len(events), 3)
        with self.assertRaises(json.JSONDecodeError):
            json.loads(events[2][1]["delta"]["partial_json"])


if __name__ == '__main__':
    unittest.main()
