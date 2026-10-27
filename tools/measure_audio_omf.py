"""Report actual Open Watcom 16-bit OMF segment bytes (not object file size).

Usage: python tools/measure_audio_omf.py build/dos/lt_audio.obj ...
"""
import json
from pathlib import Path
import sys


def index(data, pos):
    number = data[pos]
    pos += 1
    if number & 128:
        number = ((number & 127) << 8) | data[pos]
        pos += 1
    return number, pos


def segments(path):
    blob, pos, names, result = path.read_bytes(), 0, [''], []
    while pos < len(blob):
        kind = blob[pos]
        length = int.from_bytes(blob[pos+1:pos+3], 'little')
        data = blob[pos+3:pos+2+length]  # checksum excluded
        pos += 3 + length
        if kind == 0x96:  # LNAMES
            at = 0
            while at < len(data):
                size = data[at]
                at += 1
                names.append(data[at:at+size].decode('ascii'))
                at += size
        elif kind == 0x98:  # 16-bit SEGDEF
            at = 4 if data[0] >> 5 == 0 else 1
            size = int.from_bytes(data[at:at+2], 'little')
            if data[0] & 2:
                size = 65536
            name, at = index(data, at+2)
            cls, at = index(data, at)
            result.append(dict(name=names[name], segment_class=names[cls], bytes=size))
    return result


if __name__ == '__main__':
    print(json.dumps({name: segments(Path(name)) for name in sys.argv[1:]}, indent=2))
