"""Compile and run portable C89 checks for the standalone MIDI module."""
from pathlib import Path
import argparse
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--gcc-launcher', type=Path)
    args = parser.parse_args()
    wrapper = Path('C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py')
    if os.name == 'nt' and (not args.gcc_launcher or
                            args.gcc_launcher.resolve() != wrapper.resolve()):
        parser.error('Windows GCC requires the approved gcc-ucrt64.py launcher')
    cc = ([sys.executable, str(args.gcc_launcher.resolve()), '--']
          if args.gcc_launcher else [args.cc])
    out = REPO / 'build' / 'host'
    out.mkdir(parents=True, exist_ok=True)
    for name, extra, defs in (('portable_midi', ['SMF.C'], []),
                              ('bank_test', [], []),
                              ('trace_default', [], []),
                              ('trace_default', [], ['-DWITH_BANK'])):
        tag = name + ('_bank' if defs else '')
        program = out / (tag + ('.exe' if os.name == 'nt' else ''))
        command = cc + ['-x', 'c', '-std=c89', '-pedantic-errors',
                   '-Wall', '-Wextra', '-Werror',
                   '-Wno-misleading-indentation', '-Wno-sign-compare',
                   '-Wno-unused-function', '-O2', '-DSYNTH_HOST'] + defs + [
                   '-I' + str(ROOT), str(ROOT / 'INSTR.C'),
                   str(ROOT / 'MIDISYN.C')]
        command += [str(ROOT / f) for f in extra]
        command += [str(ROOT / 'tests' / (name + '.c')), '-o', str(program)]
        subprocess.run(command, check=True, timeout=30)
        subprocess.run([str(program)], check=True, timeout=30)
    audit = [sys.executable, str(ROOT/'tests/audition/run.py'), '--cc', args.cc]
    if args.gcc_launcher:
        audit += ['--gcc-launcher', str(args.gcc_launcher.resolve())]
    subprocess.run(audit, check=True, timeout=60)

if __name__ == '__main__':
    main()
