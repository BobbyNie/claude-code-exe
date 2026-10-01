"""Synthetic MCP fixture runs through real stdio; no external service approval."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class McpFixtureTests(unittest.TestCase):
    def test_stdio_discovery_call_and_notification_do_not_replay(self):
        with tempfile.TemporaryDirectory() as folder:
            evidence = Path(folder) / 'calls.jsonl'
            messages = [
                {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}},
                {'jsonrpc': '2.0', 'method': 'notifications/initialized'},
                {'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'},
                {'jsonrpc': '2.0', 'id': 3, 'method': 'tools/call',
                 'params': {'name': 'probe', 'arguments': {'marker': 'mcp-fixture-only'},
                            '_meta': {'progressToken': 'synthetic-only'}}},
                {'jsonrpc': '2.0', 'id': 4, 'method': 'tools/call',
                 'params': {'name': 'unknown', 'arguments': {}}},
            ]
            result = subprocess.run([sys.executable, str(Path(__file__).parent /
                'ccode/mcp-fixture.py'), str(evidence)],
                input=''.join(json.dumps(message) + '\n' for message in messages),
                text=True, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            responses = [json.loads(line) for line in result.stdout.splitlines()]
            self.assertEqual([response['id'] for response in responses], [1, 2, 3, 4])
            self.assertEqual(responses[1]['result']['tools'][0]['name'], 'probe')
            self.assertEqual(responses[2]['result']['content'],
                [{'type': 'text', 'text': 'mcp-fixture-only'}])
            self.assertIn('error', responses[3])
            self.assertEqual(evidence.read_text(), '{"tool":"probe","marker":"mcp-fixture-only"}\n')

    def test_acceptance_requires_discovery_real_result_and_single_server_call(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location('tools_fixture',
            Path(__file__).parent / 'ccode/tools-integration.py')
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        requests = [{'tools': [{'name': 'mcp__fixture__probe'}]}]
        received = {'acceptance_0': {'content': [{'type': 'text', 'text': 'mcp-fixture-only'}]}}
        with tempfile.TemporaryDirectory() as folder:
            evidence = Path(folder) / 'calls.jsonl'
            evidence.write_text('{"tool":"probe","marker":"mcp-fixture-only"}\n')
            fixture.verify_mcp_execution(requests, received, evidence)
            for invalid in ({}, {'acceptance_0': {'is_error': True, 'content': 'mcp-fixture-only'}}):
                with self.assertRaises(AssertionError):
                    fixture.verify_mcp_execution(requests, invalid, evidence)
            with self.assertRaises(AssertionError):
                fixture.verify_mcp_execution([{'tools': []}], received, evidence)
            evidence.write_text(evidence.read_text() * 2)
            with self.assertRaises(AssertionError):
                fixture.verify_mcp_execution(requests, received, evidence)

    def test_denied_mcp_requires_error_result_and_no_server_invocation(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location('tools_fixture',
            Path(__file__).parent / 'ccode/tools-integration.py')
        fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixture)
        requests = [{'tools': [{'name': 'mcp__fixture__probe'}]}]
        denied = {'acceptance_0': {'is_error': True, 'content': 'Permission denied'}}
        with tempfile.TemporaryDirectory() as folder:
            evidence = Path(folder) / 'calls.jsonl'
            fixture.verify_mcp_execution(requests, denied, evidence, denied=True)
            with self.assertRaises(AssertionError):
                fixture.verify_mcp_execution(requests,
                    {'acceptance_0': {'content': 'mcp-fixture-only'}}, evidence, denied=True)
            evidence.write_text('{"tool":"probe","marker":"mcp-fixture-only"}\n')
            with self.assertRaises(AssertionError):
                fixture.verify_mcp_execution(requests, denied, evidence, denied=True)
