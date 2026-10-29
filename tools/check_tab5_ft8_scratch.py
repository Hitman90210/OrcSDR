#!/usr/bin/env python3
"""Verify the task-only FT8/FT4/JS8 PCM scratch is linked in external BSS."""
import argparse
import subprocess
import sys
from elftools.elf.elffile import ELFFile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf')
    parser.add_argument('--nm', required=True)
    args = parser.parse_args()
    result = subprocess.run([args.nm, '-S', '-C', args.elf], check=True,
                            capture_output=True, text=True)
    matches = [line.split(maxsplit=3) for line in result.stdout.splitlines()
               if 'orcsdr::ft8_runtime::' in line and 'decode_slot(' in line
               and line.endswith('::chunk')]
    if len(matches) != 1:
        print('FAIL: expected exactly one shared decoder PCM scratch symbol', file=sys.stderr)
        return 1
    address, size = int(matches[0][0], 16), int(matches[0][1], 16)
    with open(args.elf, 'rb') as stream:
        elf = ELFFile(stream)
        section = elf.get_section_by_name('.ext_ram.bss')
        if section is None or size != 8192 or not (
                section['sh_addr'] <= address and
                address + size <= section['sh_addr'] + section['sh_size']):
            print(f'FAIL: 8192-byte PCM scratch must be in PSRAM; address=0x{address:x}, bytes={size}',
                  file=sys.stderr)
            return 1
    print(f'PASS: shared FT8/FT4/JS8 PCM scratch is in PSRAM: address=0x{address:x}, bytes={size}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
