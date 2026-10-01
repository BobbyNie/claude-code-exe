"""Skill acceptance must observe real loading, not just listing a name."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('tools_fixture',
    Path(__file__).parent / 'ccode/tools-integration.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class SkillFixtureTests(unittest.TestCase):
    def test_skill_body_arrives_only_after_successful_tool_execution(self):
        first = {'tools': [{'name': 'Skill'}], 'messages': [{'content': 'Use acceptance-probe'}]}
        later = {'messages': [{'content': 'skill-body-loaded-only-acceptance'}]}
        result = {'acceptance_0': {'content': 'Launching skill'}}
        fixture.verify_skill_execution([first, later], result)
        for requests, received in (([first], result),
            ([later, later], result), ([first, later], {}),
            ([first, later], {'acceptance_0': {'is_error': True}})):
            with self.assertRaises(AssertionError):
                fixture.verify_skill_execution(requests, received)
