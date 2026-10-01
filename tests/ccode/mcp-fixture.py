"""Local acceptance-only MCP server: one synthetic, read-only tool."""
import json
from pathlib import Path
import sys


def serve(evidence):
    for line in sys.stdin:
        request = json.loads(line)
        if 'id' not in request:
            continue
        response = {'jsonrpc': '2.0', 'id': request['id']}
        method = request.get('method')
        if method == 'initialize':
            response['result'] = {'protocolVersion': '2024-11-05',
                'capabilities': {'tools': {}},
                'serverInfo': {'name': 'acceptance-fixture', 'version': '1'}}
        elif method == 'ping':
            response['result'] = {}
        elif method == 'tools/list':
            response['result'] = {'tools': [{'name': 'probe',
                'description': 'Return a fixed synthetic acceptance marker',
                'inputSchema': {'type': 'object', 'properties': {
                    'marker': {'type': 'string', 'enum': ['mcp-fixture-only']}},
                    'required': ['marker'], 'additionalProperties': False}}]}
        elif (method == 'tools/call' and request.get('params') ==
              {'name': 'probe', 'arguments': {'marker': 'mcp-fixture-only'}}):
            with evidence.open('a', encoding='utf-8') as output:
                output.write('{"tool":"probe","marker":"mcp-fixture-only"}\n')
            response['result'] = {'content': [{'type': 'text', 'text': 'mcp-fixture-only'}]}
        else:
            response['error'] = {'code': -32602, 'message': 'Unsupported fixture request'}
        print(json.dumps(response), flush=True)


if __name__ == '__main__':
    serve(Path(sys.argv[1]))
