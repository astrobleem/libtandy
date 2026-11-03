"""Run built noninteractive checks in an isolated DOSBox-X Tandy guest.

Opt-in developer tool. Requires your installed emulator; downloads nothing.
Cycle settings qualify functionality, not physical 8088 clock equivalence.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
from build import TESTS

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dosbox', default='dosbox-x',
                   help='DOSBox-X executable or isolated wrapper')
    p.add_argument('--model', choices=('small', 'compact', 'medium', 'large'),
                   default='small')
    p.add_argument('--cycles', type=int, default=20000)
    p.add_argument('--timeout', type=int, default=600)
    p.add_argument('--program', action='append', help='8-character EXE name; repeatable')
    a = p.parse_args()
    if a.cycles < 1 or a.timeout < 1:
        p.error('cycles and timeout must be positive')
    names = a.program or [*TESTS, 'DOSAPP', 'GUARD']
    if any(not re.fullmatch(r'[A-Za-z0-9_]{1,8}', n) for n in names):
        p.error('program names must be plain 1..8-character DOS base names')
    source = ROOT / 'build' / ('dos' if a.model == 'small' else 'dos-' + a.model)
    out = ROOT / 'build' / ('qualify-' + a.model)
    out.mkdir(parents=True, exist_ok=True)
    for n in names:
        src = source / (n + '.EXE')
        if not src.is_file():
            raise SystemExit('Missing build: ' + str(src))
        shutil.copyfile(src, out / (n.upper() + '.EXE'))
        (out / (n.upper() + '.LOG')).unlink(missing_ok=True)
    (out / 'RESULTS.TXT').unlink(missing_ok=True)
    commands = ['@echo off', 'mount c "' + str(out) + '"', 'c:']
    for n in names:
        commands += [n + ' > ' + n + '.LOG', 'if errorlevel 1 goto failed',
                     'echo PASS ' + n + ' >> RESULTS.TXT']
    commands += ['echo COMPLETE >> RESULTS.TXT', 'goto done', ':failed',
                 'echo FAILED >> RESULTS.TXT', ':done', 'exit']
    cfg = out / 'checks.conf'
    cfg.write_text('''[sdl]
fullscreen=false
output=surface
[dosbox]
machine=tandy
memsize=0
memsizekb=640
splash logo=false
show basic information=false
[cpu]
core=normal
cputype=8086_prefetch
cycles=fixed %d
fpu=false
[dos]
xms=false
ems=false
umb=false
[dosbox-x]
automount=false
automountall=false
startcmd=false
[mixer]
nosound=true
[midi]
mididevice=none
[joystick]
joysticktype=none
[autoexec]
''' % a.cycles + '\n'.join(commands) + '\n')
    manifest = {'status': 'running', 'model': a.model, 'cycles': a.cycles,
                'machine': 'tandy', 'memory_kb': 640, 'programs': names,
                'sha256': {n: hashlib.sha256((out / (n.upper() + '.EXE')).read_bytes()).hexdigest()
                           for n in names}}
    record = out / 'manifest.json'
    record.write_text(json.dumps(manifest, indent=2) + '\n')
    home = out / 'home'
    home.mkdir(exist_ok=True)
    env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / '.config'),
               SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    try:
        with (out / 'emulator.log').open('w') as log:
            run = subprocess.run([a.dosbox, '-conf', str(cfg)], cwd=out,
                                 stdout=log, stderr=subprocess.STDOUT,
                                 env=env, timeout=a.timeout + 10)
        result = (out / 'RESULTS.TXT').read_text() if (out / 'RESULTS.TXT').exists() else ''
        passed = run.returncode == 0 and 'COMPLETE' in result and 'FAILED' not in result
        manifest.update(status='passed' if passed else 'failed',
                        exit_code=run.returncode, results=result)
    except BaseException as error:
        manifest.update(status='interrupted' if isinstance(error, KeyboardInterrupt) else 'failed', error=str(error))
        record.write_text(json.dumps(manifest, indent=2) + '\n')
        raise
    record.write_text(json.dumps(manifest, indent=2) + '\n')
    print(result, end='')
    if not passed:
        raise SystemExit('DOS checks failed; inspect ' + str(out))


if __name__ == '__main__':
    main()
