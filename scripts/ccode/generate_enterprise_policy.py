"""Generate native signer policy from public SPKI and independently supplied pin.

This verifies consistency, not organizational approval. Never reads a private key.
The output is a build input, not a candidate-supplied runtime trust override.
"""
import argparse
import hashlib
from pathlib import Path
import re
import stat
import sys

PREFIX = bytes.fromhex('302a300506032b6570032100')


def generate(spki, approved_pin, output):
    if not re.fullmatch(r'[0-9a-f]{64}', approved_pin):
        raise ValueError('E_SIGNER_POLICY')
    source = Path(spki)
    before = source.lstat()
    if not stat.S_ISREG(before.st_mode) or before.st_nlink != 1:
        raise ValueError('E_SIGNER_POLICY')
    with source.open('rb') as stream:
        key = stream.read(45)
    if len(key) != 44 or not key.startswith(PREFIX) or hashlib.sha256(key).hexdigest() != approved_pin:
        raise ValueError('E_SIGNER_POLICY')
    values = ', '.join(f'0x{byte:02x}' for byte in key)
    header = ('#pragma once\n#include <array>\n'
              '// Build-time public policy; approval must be recorded independently.\n'
              'namespace ccode { namespace enterprise_policy {\n'
              f'inline constexpr std::array<unsigned char, 44> SignerSpki = {{{values}}};\n'
              f'inline constexpr char SignerPin[] = "{approved_pin}";\n'
              '} }\n')
    with Path(output).open('x', encoding='ascii', newline='\n') as stream:
        stream.write(header)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--spki', required=True)
    parser.add_argument('--approved-pin', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    try:
        generate(args.spki, args.approved_pin, args.output)
    except (OSError, ValueError):
        print('E_SIGNER_POLICY', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
