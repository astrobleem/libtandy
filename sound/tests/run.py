"""Compile and run portable C89 checks for the standalone MIDI module."""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    out = REPO / 'build' / 'host'
    out.mkdir(parents=True, exist_ok=True)
    program = out / ('portable_midi.exe' if os.name == 'nt' else 'portable_midi')
    command = [args.cc, '-x', 'c', '-std=c89', '-pedantic-errors', '-Wall', '-Wextra',
               '-Werror', '-Wno-misleading-indentation', '-Wno-sign-compare',
               '-Wno-unused-function', '-O2', '-DSYNTH_HOST', '-I' + str(ROOT),
               str(ROOT / 'INSTR.C'), str(ROOT / 'MIDISYN.C'),
               str(ROOT / 'SMF.C'), str(ROOT / 'tests/portable_midi.c'),
               '-o', str(program)]
    subprocess.run(command, check=True, timeout=30)
    subprocess.run([str(program)], check=True, timeout=30)

if __name__ == '__main__':
    main()
