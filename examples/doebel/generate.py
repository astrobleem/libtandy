"""Developer-only score-header regeneration; playback never needs Python.

python examples/doebel/generate.py --check
python examples/doebel/generate.py --write   # after intentional score edits
The approved trace/audit is a frozen listening reference, not regenerated.
"""
import argparse
import json
import math
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent


def render():
    score = json.loads((HERE / 'DOEBEL.json').read_text(encoding='utf-8'))
    result = ['/* Optional PSG reduction of the referenced Evil Jingle opening.',
              ' * Musical attribution and usage: README.md.',
              ' * Generated from editable DOEBEL.json; developer-only generator.',
              ' */', '#ifndef DOEBEL_SCORE_H', '#define DOEBEL_SCORE_H',
              '#include "lt_score.h"',
              '#define DOEBEL_SERVICE_HZ %uU' % score['service_hz'],
              '#define DOEBEL_END_TICK %uU' % score['one_shot_end_tick']]
    for voice in score['voices']:
        result.append('static const lt_score_note doebel_%u[] = {' % voice['channel'])
        previous = 0
        for note in voice['notes']:
            match = re.fullmatch(r'([A-G])(#?)([0-8])', note['pitch'])
            if not match:
                raise ValueError(note['pitch'])
            midi = (int(match[3])+1)*12 + dict(C=0, D=2, E=4, F=5,
                    G=7, A=9, B=11)[match[1]] + bool(match[2])
            hz = 440 * 2 ** ((midi-69)/12)
            period = math.floor(score['psg_clock_hz']/(32*hz)+.5)
            if not (1 <= period <= 1023 and previous <= note['start'] <
                    note['end'] <= score['silent_loop_start_tick']):
                raise ValueError(note)
            previous = note['end']
            result.append('    { %uU, %uU, %uU }, /* %s %s */' % (
                note['start'], note['end'], period, note['id'], note['pitch']))
        result.append('};')
    result += ['static const lt_score doebel_score = {',
               '    LTSCORE_VERSION, 4U, %uU, %uU, %uU, %uUL,' % (
                   score['ppq'], score['silent_loop_start_tick'],
                   score['one_shot_end_tick'], score['tempo_us']),
               '    { doebel_0, doebel_1, doebel_2 },']
    for field in ('notes', 'attenuation', 'release_gap_ticks'):
        values = [(len(v[field]) if field == 'notes' else v[field]) for v in score['voices']]
        result.append('    { ' + ', '.join('%uU' % v for v in values) + ' },')
    result += ['    0, 0U, 0, 0U', '};', '#endif', '']
    return '\n'.join(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--write', action='store_true')
    args = parser.parse_args()
    expected = render()
    path = HERE / 'DOEBEL.H'
    if args.write:
        path.write_bytes(expected.replace('\n', '\r\n').encode('ascii'))
    elif path.read_text(encoding='ascii') != expected:
        raise SystemExit('FAIL DOEBEL.H does not match editable score')
    print('PASS editable Doebel score and checked-in header')


if __name__ == '__main__':
    main()
