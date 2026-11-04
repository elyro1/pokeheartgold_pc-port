#!/usr/bin/env python3
"""Generate the DS overlay ordinal IDs from the checked-in linker specification."""
import argparse
from pathlib import Path
import re


def overlay_names(source):
    names = re.findall(r'^Overlay\s+([A-Za-z_]\w*)\s*$', source, re.M)
    if not names or len(set(names)) != len(names):
        raise ValueError('overlay names must be nonempty and unique')
    for ordinal, name in enumerate(names):
        if name.startswith('OVY_') and name[4:].isdigit() and int(name[4:]) != ordinal:
            raise ValueError(f'{name} has ordinal {ordinal}, inconsistent with its explicit number')
    return names


def generate(source):
    names = overlay_names(source)
    lines = ['/* Generated from main.lsf; IDs are ordinals, never native addresses. */',
             '#ifndef HG_PC_OVERLAY_IDS_H', '#define HG_PC_OVERLAY_IDS_H']
    lines.extend(f'#define HG_PC_OVERLAY_ID_{name} {ordinal}u' for ordinal, name in enumerate(names))
    lines.extend([f'#define HG_PC_OVERLAY_COUNT {len(names)}u', '#endif', ''])
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    result = generate(args.source.read_text())
    if not args.output.exists() or args.output.read_text() != result:
        args.output.write_text(result)
