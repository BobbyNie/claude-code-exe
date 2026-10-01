"""Failure evidence exposes structural process outcomes, not private output."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    'concurrency_fixture', Path(__file__).parent / 'ccode/concurrency-integration.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class ConcurrencyFixtureTests(unittest.TestCase):
    def test_parallel_failure_reports_exit_codes_without_private_output(self):
        message = fixture.parallel_failure_evidence(
            1, [0, 75], [('private prompt token', ''),
                        ('private path', 'E_PROFILE_BUSY\nsecret token')])
        self.assertIn('requests=1', message)
        self.assertIn('exit_codes=[0, 75]', message)
        self.assertIn("neutral_codes=[[], ['E_PROFILE_BUSY']]", message)
        for private in ('private', 'prompt', 'secret', 'token', 'path'):
            self.assertNotIn(private, message)
