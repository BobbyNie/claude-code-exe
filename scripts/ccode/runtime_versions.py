"""Resolve the official latest/stable channels once for every acceptance job."""
import json
import os
from pathlib import Path
import re
from urllib.request import urlopen

BASE = ('https://storage.googleapis.com/'
        'claude-code-dist-86c565f3-f756-42ad-8dfa-d59b1c096819/claude-code-releases')


def fetch_version(channel):
    with urlopen(f'{BASE}/{channel}', timeout=30) as response:
        return response.read(256).decode('utf-8-sig').strip()


def resolve():
    latest, stable = fetch_version('latest'), fetch_version('stable')
    for value in (latest, stable):
        if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', value):
            raise ValueError('Invalid official engine version')
    if tuple(map(int, stable.split('.'))) > tuple(map(int, latest.split('.'))):
        raise ValueError('Stable channel is newer than latest; refusing inconsistent snapshot')
    return {'latest': latest, 'stable': stable,
            'versions': list(dict.fromkeys([stable, latest])), 'cross_version': stable != latest}


if __name__ == '__main__':
    snapshot = resolve()
    if os.environ.get('GITHUB_OUTPUT'):
        with Path(os.environ['GITHUB_OUTPUT']).open('a', encoding='utf-8') as output:
            for key, value in snapshot.items():
                encoded = value if isinstance(value, str) else json.dumps(value, separators=(',', ':'))
                output.write(f'{key}={encoded}\n')
    print(json.dumps(snapshot))
