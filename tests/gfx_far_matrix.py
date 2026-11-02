#!/usr/bin/env python3
"""Opt-in DOS far-pointer boundary checks; installed Linux Watcom/DOSBox-X only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--watcom',type=Path,required=True)
p.add_argument('--dosbox',type=Path,required=True,help='DOSBox-X executable or environment wrapper')
p.add_argument('--timeout',type=int,default=90)
a=p.parse_args(); root=Path(__file__).resolve().parents[1]
out=root/'build/gfx-far-matrix';out.mkdir(parents=True,exist_ok=True)
w=a.watcom.resolve();bindir=w/('binl64' if (w/'binl64/wcl').exists() else 'binl')
env=dict(os.environ,WATCOM=str(w),INCLUDE=str(w/'h'),LIB=str(w/'lib286')+':'+str(w/'lib286/dos'))
env['PATH']=str(bindir)+os.pathsep+env.get('PATH','')
sources=[root/'src/lt_gfx.c',root/'src/lt_gfx_tile8.c',root/'tests/gfx_far_dos.c']
records=[];names=[]
for model in 'scml':
    for kind in ('A','C'):
        name='FAR'+model.upper()+kind;names.append(name)
        flags=['-q','-0','-m'+model,'-ecc','-bt=dos','-ox','-s','-zt=0','-w4','-we','-k8192','-i='+str(root/'include'),'-fe='+name+'.EXE']
        if kind=='C':flags+=['-dLT_GFX_TILE8_C_ONLY','-dLT_GFX_C_FILL']
        cmd=[str(bindir/'wcl')]+flags+list(map(str,sources))
        r=subprocess.run(cmd,cwd=out,env=env,text=True,capture_output=True,timeout=30)
        records.append(dict(argv=cmd,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
        (out/'compile.json').write_text(json.dumps(records,indent=2)+'\n')
        r.check_returncode()
config='''[sdl]
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
cycles=fixed 20000
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
'''
config+='mount c "'+str(out)+'"\nc:\n'
for name in names:
    (out/(name+'.LOG')).unlink(missing_ok=True)
    config+=name+' > '+name+'.LOG\n'
config+='exit\n';(out/'far.conf').write_text(config)
(out/'home').mkdir(exist_ok=True)
env.update(HOME=str(out/'home'),XDG_CONFIG_HOME=str(out/'home/.config'),SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
with (out/'emulator.log').open('w') as log:
    subprocess.run([str(a.dosbox.resolve()),'-defaultconf','-conf',str(out/'far.conf')],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=a.timeout,check=True)
for name in names:
    log=(out/(name+'.LOG')).read_text()
    if 'PASS far graphics cases=64 failures=0' not in log or 'FAIL' in log:raise SystemExit(name+' failed: '+log)
    print(name+': '+log.strip())
files=sources+[root/'include/lt_gfx.h',root/'include/lt_gfx_tile8.h']+[out/(n+'.EXE') for n in names]
(out/'manifest.json').write_text(json.dumps({str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},indent=2)+'\n')
