#!/usr/bin/env python3
"""Build/run the DOS IRQ ABI matrix using installed tools (no downloads)."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--watcom', required=True, type=Path)
p.add_argument('--dosbox', required=True, type=Path, help='DOSBox-X executable or environment-setting wrapper')
p.add_argument('--build', type=Path, default=Path('build/tick-matrix'))
p.add_argument('--cycles', type=int, default=20000)
p.add_argument('--timeout', type=int, default=90)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
build = a.build.resolve(); build.mkdir(parents=True, exist_ok=True)
watcom = a.watcom.resolve()
wcl = next((watcom / d / 'wcl' for d in ('binl64', 'binl') if (watcom / d / 'wcl').exists()), None)
if wcl is None:
    raise SystemExit('No installed Linux Watcom wcl found')
env = dict(os.environ, WATCOM=str(watcom), INCLUDE=str(watcom/'h'),
           LIB=str(watcom/'lib286')+':'+str(watcom/'lib286/dos'))
env['PATH'] = str(wcl.parent)+os.pathsep+env.get('PATH','')
flags = ['-q','-0','-ecc','-bt=dos','-ox','-s','-zt=0','-w4','-we','-dLTTK_TEST','-k8192','-i='+str(root/'include')]
sources = [root/'adapters/lt_tick_dos.c', root/'tests/tick_dos.c']
for model in 'scml':
    cmd = [str(wcl), '-m'+model]+flags+['-fe=TICK'+model+'.EXE']+list(map(str,sources))
    result = subprocess.run(cmd, cwd=build, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (build/('compile-'+model+'.log')).write_text(result.stdout)
    result.check_returncode()
# The adapter must reject stack probes rather than silently compiling an unsafe ISR.
cmd = [str(wcl), '-ms']+[x for x in flags if x not in ('-s','-ox')]+['-od']+['-c',str(sources[0])]
r = subprocess.run(cmd,cwd=build,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(build/'reject-stack-probes.log').write_text(r.stdout)
if r.returncode == 0 or 'Compile lt_tick_dos.c' not in r.stdout:
    raise SystemExit('Missing -s was not rejected as expected')
conf = '''[sdl]
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
cycles=fixed {cycles}
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
@echo off
mount c "{directory}"
c:
'''.format(cycles=a.cycles,directory=build)
for model in 'scml':
    log = build/('TICK'+model.upper()+'.LOG')
    if log.exists(): log.unlink()  # only this runner's disposable logs
    conf += 'TICK'+model+' > TICK'+model+'.LOG\n'
conf += 'exit\n'
(build/'matrix.conf').write_text(conf)
(build/'home').mkdir(exist_ok=True)
env.update(HOME=str(build/'home'),SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
with (build/'emulator.log').open('w') as log:
    subprocess.run([str(a.dosbox.resolve()),'-defaultconf','-conf',str(build/'matrix.conf')],cwd=build,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=a.timeout,check=True)
for model in 'scml':
    log=(build/('TICK'+model.upper()+'.LOG')).read_text()
    if 'PASS timer checks=' not in log or 'FAIL' in log:
        raise SystemExit('Timer model '+model+' failed: '+log)
    print(model+': '+log.strip().splitlines()[-1])
files = sources+[root/'include/lt_tick_dos.h']+list(build.glob('*.EXE'))
manifest={str(f.relative_to(root) if f.is_relative_to(root) else f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
(build/'manifest.json').write_text(json.dumps({'flags':flags,'cycles':a.cycles,'sha256':manifest},indent=2)+'\n')
