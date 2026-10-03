"""Cancellation evidence must await the request and preserve cleanup failures."""
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import Mock

spec = importlib.util.spec_from_file_location('bridge_fixture',
    Path(__file__).parent / 'ccode/gateway-bridge-integration.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class CancellationFixtureTests(unittest.TestCase):
    def test_cancellation_waits_for_request_and_requires_clean_exit(self):
        process = Mock(returncode=0)
        process.communicate.return_value = ('', '')
        pending = Mock()
        pending.result.side_effect = ConnectionResetError()
        started = Mock()
        started.wait.return_value = True
        fixture.cancel_request(process, pending, started)
        started.wait.assert_called_once_with(5)
        process.communicate.assert_called_once_with('stop\n', timeout=5)
        pending.result.assert_called_once_with(timeout=5)
        for code, output in ((1, ('', '')), (0, ('E_NETWORK', '')), (0, ('', 'private'))):
            process.returncode = code
            process.communicate.return_value = output
            with self.assertRaises(AssertionError):
                fixture.cancel_request(process, pending, started)

    def test_cancellation_does_not_swallow_timeout_or_unexpected_request_errors(self):
        process = Mock(returncode=0)
        process.communicate.return_value = ('', '')
        started = Mock()
        started.wait.return_value = True
        for error in (TimeoutError(), ValueError('unexpected')):
            pending = Mock()
            pending.result.side_effect = error
            with self.assertRaises(type(error)):
                fixture.cancel_request(process, pending, started)
