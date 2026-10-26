"""Developer-only extraction/check of OutRun SE's supplied Sega Tune VGM.
No runtime dependency. Does not modify the input or any OutRun project file.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA = '8f4961245386c32595b2288238ca7567d144757abe151d28b35a9968727ea2b1'

def extract(path):
    b = path.read_bytes()
    assert hashlib.sha256(b).hexdigest() == SOURCE_SHA, 'Different source VGM'
    assert b[:4] == b'Vgm ' and struct.unpack_from('<I', b, 8)[0] == 0x101
    assert struct.unpack_from('<I', b, 12)[0] == 3579540
    i, sample, events = 64, 0, []
    while True:
        op = b[i]; i += 1
        if op == 0x50:
            events.append((sample, b[i])); i += 1
        elif op == 0x4f:
            assert b[i] == 255; i += 1
        elif op == 0x61:
            sample += int.from_bytes(b[i:i+2], 'little'); i += 2
        elif op == 0x62: sample += 735
        elif op == 0x66: break
        else: raise ValueError(hex(op))
    assert sample == 147826 and len(events) == 710
    return events

def tick(sample):
    return (sample * 1193182 + 44100 * 4972 - 1) // (44100 * 4972)

def outputs(events):
    groups = []
    for sample, value in events:
        at = tick(sample)
        if not groups or groups[-1][0] != at: groups.append((at, []))
        groups[-1][1].append(value)
    assert max(len(g[1]) for g in groups) <= 12
    data = []
    for at, values in groups: data += [at & 255, at >> 8, len(values), *values]
    h = '/* Generated PSG bytes; musical data: Sega BIOS, logged by Maxim. */\n'
    h += 'static const lt_u8 sms_boot_data[] = {\n'
    h += ''.join('    '+', '.join('0x%02XU' % v for v in data[i:i+12])+',\n'
                 for i in range(0, len(data), 12)) + '};\n'
    csv = 'sample_44100,psg_byte_hex\n' + ''.join('%d,%02X\n' % e for e in events)
    return {'src/lt_sms_boot_data.h': h, 'examples/smsboot/REFERENCE.csv': csv}, groups, data

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('vgm', type=Path)
    p.add_argument('--write', action='store_true')
    a = p.parse_args()
    files, groups, data = outputs(extract(a.vgm))
    for name, text in files.items():
        dest = ROOT / name
        if a.write:
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(text, encoding='ascii', newline='')
        else:
            assert dest.read_text(encoding='ascii') == text, name
    print(json.dumps({'writes':710, 'groups':len(groups), 'table_bytes':len(data),
                      'final_tick':tick(88291), 'end_tick':tick(147826)}))

if __name__ == '__main__': main()
