"""Subagent acceptance requires child context and parent tool result evidence."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('subagent_tools_fixture',
    Path(__file__).parent / 'ccode/tools-integration.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class SubagentFixtureTests(unittest.TestCase):
    def test_subagent_requires_independent_child_request_and_successful_result(self):
        parent = {'tools': [{'name': 'Agent'}], 'system': 'parent-only',
                  'messages': [{'content': 'Use acceptance-probe'}]}
        child = {'system': 'child-context-only-acceptance', 'messages': []}
        received = {'acceptance_0': {'content': 'child-result-only-acceptance'}}
        returned = {'system': 'parent-only', 'messages': [{'content': [
            {'type': 'tool_result', 'tool_use_id': 'acceptance_0',
             'content': 'child-result-only-acceptance'}]}]}
        fixture.verify_subagent_execution([parent, child, returned], received, 'Agent')
        with self.assertRaises(AssertionError):
            fixture.verify_subagent_execution([parent, child], received, 'Agent')
        with self.assertRaises(AssertionError):
            fixture.verify_subagent_execution([parent, dict(child, messages=returned['messages'])],
                                             received, 'Agent')
        cases = [([parent], received), ([child, child], received),
                 ([parent, child], {}),
                 ([parent, child], {'acceptance_0': {'is_error': True,
                    'content': 'child-result-only-acceptance'}}),
                 ([parent, child], {'acceptance_0': {'content': 'missing'}}),
                 ([parent, {'messages': [{'content': 'child-context-only-acceptance'}]}], received)]
        for requests, results in cases:
            with self.assertRaises(AssertionError):
                fixture.verify_subagent_execution(requests, results, 'Agent')

    def test_native_registry_summary_exposes_only_fixed_name_flags(self):
        events = [{'type': 'system', 'subtype': 'init', 'tools': ['Task', 'private-tool']},
                  {'type': 'assistant', 'message': {'content': [
                    {'type': 'tool_use', 'name': 'Agent', 'input': {'prompt': 'private-prompt'}}]}}]
        self.assertEqual(fixture.summarize_subagent_registry(events), {
            'init_agent': False, 'init_task': True, 'emitted_agent': True,
            'emitted_task': False, 'emitted_unregistered': True})
        self.assertNotIn('private', str(fixture.summarize_subagent_registry(events)))

    def test_foreground_subagent_arguments_preserve_background_default_case(self):
        base = {'subagent_type': 'acceptance-probe', 'description': 'probe', 'prompt': 'probe'}
        self.assertEqual(fixture.subagent_arguments(base, foreground=False), base)
        self.assertEqual(fixture.subagent_arguments(base, foreground=True),
                         dict(base, run_in_background=False))
        self.assertNotIn('run_in_background', base)

    def test_native_order_summary_does_not_expose_event_content(self):
        events = [{'type': 'assistant', 'message': {'content': 'private'}},
                  {'type': 'result'}, {'type': 'assistant'}, {'type': 'result'}]
        self.assertEqual(fixture.summarize_subagent_order(events),
                         {'result_count': 2, 'assistant_after_result': True})

    def test_select_subagent_tool_requires_actual_schema_not_name_only(self):
        for name in ('Agent', 'Task'):
            tools = [{'name': name, 'input_schema': {'properties': {
                'subagent_type': {'type': 'string'}, 'prompt': {'type': 'string'},
                'description': {'type': 'string'}}}}]
            self.assertEqual(fixture.select_subagent_tool(tools), name)
        for tools in ([], [{'name': 'Agent'}], [{'name': 'Bash'}]):
            with self.assertRaises(AssertionError):
                fixture.select_subagent_tool(tools)
