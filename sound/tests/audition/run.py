"""Check each expected note's quantized PCM contribution in the actual mixer.

No WAVs are written. --audio also compares all eight takes with existing WAVs.
The deliberately muted-note control must fail despite nonzero mixed output.
"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import sys

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
SOURCE = REPO/'sound'
WRAPPER = Path('C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--gcc-launcher', type=Path)
    parser.add_argument('--audio', type=Path)
    parser.add_argument('--out', type=Path, default=REPO/'build/host/audnotes')
    args = parser.parse_args()
    if os.name == 'nt' and (not args.gcc_launcher or
                            args.gcc_launcher.resolve() != WRAPPER.resolve()):
        parser.error('Windows GCC requires the approved gcc-ucrt64.py launcher')
    cc = ([sys.executable, str(args.gcc_launcher.resolve()), '--']
          if args.gcc_launcher else [args.cc])
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = (REPO/'tools/psgaudit.c').read_text()

    def change(old, new):
        nonlocal source
        if source.count(old) != 1:
            raise ValueError('Audition mixer changed; review instrumentation: '+old)
        source = source.replace(old, new)

    change('static int RenderBank(unsigned bank, RENDER *r)',
           '#include "AUDHOOK.H"\nstatic int RenderBank(unsigned bank, RENDER *r)')
    change('if (MSMessage(&synth, ms, m)) r->rejected++;',
           'if (AudEvent(ei, ms, m, MSMessage(&synth, ms, m))) r->rejected++;')
    change('sum = 0.0;', 'AudSampleBegin();sum = 0.0;')
    change('sum += (tp[i] < 0.5 ? 1.0 : -1.0)\n'
           '                    * Amp(synth.volume[i]) * TONE_GAIN;',
           'sum += AudPart(i,(tp[i] < 0.5 ? 1.0 : -1.0)\n'
           '                    * Amp(synth.volume[i]) * TONE_GAIN);')
    change('sum += ((lfsr & 1U) ? 1.0 : -1.0)\n'
           '                * Amp(synth.volume[3]) * NOISE_GAIN;',
           'sum += AudPart(3,((lfsr & 1U) ? 1.0 : -1.0)\n'
           '                * Amp(synth.volume[3]) * NOISE_GAIN);')
    change('r->data[n] = s;', 'r->data[n] = s;AudCommit(n,s,sum);')
    change('int main(int argc, char **argv)',
           'static int OriginalMain(int argc, char **argv)')
    source += '\n#include "AUDMAIN.H"\n'
    private_source = out/'psgaudit.c'
    private_source.write_text(source, newline='\n')
    exe = out/('audnotes.exe' if os.name == 'nt' else 'audnotes')
    records = []

    def run(command, expected=0):
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        records.append({'command': command, 'returncode': result.returncode,
                        'stdout': result.stdout, 'stderr': result.stderr})
        (out/'results.json').write_text(json.dumps(records, indent=2)+'\n')
        print(result.stdout, end='')
        print(result.stderr, end='', file=sys.stderr)
        if result.returncode != expected:
            raise RuntimeError('Unexpected result: '+str(result.returncode))
        return result.stdout

    run(cc+['-x', 'c', '-std=c89', '-pedantic-errors', '-Wall', '-Wextra',
            '-Werror', '-Wno-unused-function', '-Wno-misleading-indentation',
            '-Wno-sign-compare', '-O2', '-DSYNTH_HOST', '-I', str(SOURCE),
            '-I', str(HERE), str(private_source), str(SOURCE/'INSTR.C'),
            str(SOURCE/'MIDISYN.C'), '-o', str(exe)])
    audio = str(args.audio.resolve()) if args.audio else '-'
    positive = run([str(exe), audio, str(out/'pernote.csv')])
    if 'notes=154 missing=0' not in positive:
        raise RuntimeError('Missing positive per-note result')
    negative = run([str(exe), audio, str(out/'muted.csv'), '--muted-note'], 1)
    if 'EXPECTED_NEGATIVE_DETECTED' not in negative:
        raise RuntimeError('Muted-note negative control did not detect silence')


if __name__ == '__main__':
    main()
