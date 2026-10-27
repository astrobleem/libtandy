"""Compile original logical PSG JSON to an immutable C89 lt_stream descriptor.

Developer tool only. Consumers compile the generated header, needing no Python.
Commands: ["tone", channel, period, level], ["noise", mode, level],
["level", channel, level], ["stop", mask], ["wait", ticks].
Compression refers only to previously emitted whole literal commands, depth one.
The runtime validator remains authoritative for timing/ownership/step limits.
"""
import argparse
import json
from pathlib import Path
import re


def word(n):
    if not 0 <= n <= 65535:
        raise ValueError('16-bit field out of range')
    return bytes((n & 255, n >> 8))


def encode(c):
    op, *a = c
    if op == 'tone' and len(a) == 3 and 0 <= a[0] <= 2 and 0 <= a[1] <= 1023 and 0 <= a[2] <= 15:
        return bytes((0x10 + a[0],)) + word(a[1]) + bytes((a[2],))
    if op == 'noise' and len(a) == 2 and 0 <= a[0] <= 7 and 0 <= a[1] <= 15:
        return bytes((0x13, *a))
    if op == 'level' and len(a) == 2 and 0 <= a[0] <= 3 and 0 <= a[1] <= 15:
        return bytes((0x20 + a[0], a[1]))
    if op == 'stop' and len(a) == 1 and 1 <= a[0] <= 15:
        return bytes((0x30, a[0]))
    if op == 'wait' and len(a) == 1 and 1 <= a[0] <= 65535:
        return b'\x01' + word(a[0])
    raise ValueError('Invalid command: ' + repr(c))


def compile_commands(commands, compress=True):
    records = [encode(c) for c in commands]
    out = bytearray()
    # Literal runs only: references can never refer to other references.
    runs, run, i = [], [], 0
    while i < len(records):
        best = None
        if compress:
            for literals in runs + ([run] if run else []):
                for start in range(len(literals)):
                    for end in range(start + 1, len(literals) + 1):
                        block = [r for _, r in literals[start:end]]
                        size = sum(map(len, block))
                        count = 0
                        while count < 255 and records[i + count * len(block):i + (count + 1) * len(block)] == block:
                            count += 1
                        saving = size * count - 6
                        if saving > 0 and (best is None or saving > best[0]):
                            best = saving, literals[start][0], size, count, len(block)
        if best:
            _, offset, size, count, length = best
            out += b'\x02' + word(offset) + word(size) + bytes((count,))
            if run:
                runs.append(run)
                run = []
            i += count * length
        else:
            run.append((len(out), records[i]))
            out += records[i]
            i += 1
    out += b'\x00'
    if len(out) > 32767:
        raise ValueError('Near stream exceeds 32767 bytes')
    return bytes(out), sum(map(len, records)) + 1


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--plain', action='store_true')
    a = p.parse_args()
    source = json.loads(a.source.read_text())
    name = source['name']
    if not re.fullmatch('[a-z][a-z0-9_]*', name):
        raise ValueError('C identifier required')
    data, plain = compile_commands(source['commands'], not a.plain)
    mask, priority = source['channel_mask'], source.get('priority', 0)
    if not 1 <= mask <= 15 or not 0 <= priority <= 255:
        raise ValueError('Invalid channel mask or priority')
    clock, num, den = source['clock_hz'], source['rate_num'], source['rate_den']
    if not all(1 <= n <= 0xffffffff for n in (clock, num, den)):
        raise ValueError('Invalid metadata')
    lines = ['/* SPDX-License-Identifier: MIT */', '/* Generated from ' + a.source.name + '; no runtime conversion. */', '#include "lt_stream.h"',
             'static const lt_u8 ' + name + '_data[] = {']
    lines += ['    ' + ','.join('0x%02X' % b for b in data[i:i+12]) + ',' for i in range(0, len(data), 12)]
    lines += ['};', 'static const lt_stream ' + name + ' = {',
              '    ' + name + '_data, sizeof(' + name + '_data),',
              f'    LTST_VERSION, LTST_TANDY, {mask}, {priority},',
              f'    {clock}UL, {num}UL, {den}UL', '};', '']
    a.output.write_text('\n'.join(lines))
    print(f'{name}: plain={plain} packed={len(data)} bytes (descriptor excluded)')


if __name__ == '__main__':
    main()
