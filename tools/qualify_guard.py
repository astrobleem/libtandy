"""Build/run optional DOS guard checks in all four 8088 memory models.

Opt-in. Uses installed Open Watcom and DOSBox-X; downloads nothing.
Software interrupts only: no real device errors are injected.
"""
from pathlib import Path
import argparse, json, os, subprocess, hashlib
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--watcom',type=Path,required=True)
    p.add_argument('--dosbox',default='dosbox-x')
    a=p.parse_args();w=a.watcom.resolve()
    out=ROOT/'build'/'guard-matrix';out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ)
    if os.name=='nt':
        bindir=w/'BINNT';cc=bindir/'wcl.exe';env.update(WATCOM=str(w),INCLUDE=str(w/'H'),LIB=str(w/'LIB286')+';'+str(w/'LIB286/DOS'))
    else:
        bindir=w/('binl64' if (w/'binl64/wcl').exists() else 'binl');cc=bindir/'wcl'
        env.update(WATCOM=str(w),INCLUDE=str(w/'h'),LIB=str(w/'lib286')+os.pathsep+str(w/'lib286/dos'))
    env['PATH']=str(bindir)+os.pathsep+env.get('PATH','')
    record={'status':'building','tests':[]}
    def run(args,expected=0):
        r=subprocess.run(list(map(str,args)),cwd=out,env=env,capture_output=True,text=True,timeout=60)
        record['tests'].append({'argv':list(map(str,args)),'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        if (r.returncode==0)!=(expected==0): raise RuntimeError(r.stdout+r.stderr)
    try:
        for model in 'scml':
            flags=[cc,'-q','-bt=dos','-0','-m'+model,'-ecc','-ox','-w4','-wx','-i='+str(ROOT/'include')]
            run([*flags,'-s','-zu','-zt=0','-c','-fo=guard'+model+'.obj',ROOT/'adapters/lt_guard_dos.c'])
            run([*flags,'-s','-k4096','-fe=GUARD'+model.upper()+'.EXE',ROOT/'tests/guard_dos.c','guard'+model+'.obj'])
        for absent in ('-s','-zu'):
            flags=[cc,'-q','-bt=dos','-0','-ml','-ecc','-s','-zu','-i='+str(ROOT/'include'),'-c','-fo=invalid.obj',ROOT/'adapters/lt_guard_dos.c']
            run([x for x in flags if x!=absent],expected=1)
        commands=['@echo off','mount c "'+str(out)+'"','c:']
        for model in 'SCML':
            (out/('GUARD'+model+'.LOG')).unlink(missing_ok=True)
            commands+=['GUARD'+model+' > GUARD'+model+'.LOG','if errorlevel 1 goto failed']
        commands+=['echo COMPLETE > RESULT.TXT','goto done',':failed','echo FAILED > RESULT.TXT',':done','exit']
        (out/'RESULT.TXT').unlink(missing_ok=True)
        cfg=out/'guard.conf'
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
'''+ '\n'.join(commands)+'\n')
        home=out/'home';home.mkdir(exist_ok=True)
        env.update(HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
        with (out/'emulator.log').open('w') as log:
            r=subprocess.run([a.dosbox,'-conf',str(cfg)],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=60)
        if r.returncode or not (out/'RESULT.TXT').exists() or 'COMPLETE' not in (out/'RESULT.TXT').read_text():
            raise RuntimeError('DOS guard matrix failed; inspect '+str(out))
        record['status']='passed'
        record['logs']={m:(out/('GUARD'+m+'.LOG')).read_text() for m in 'SCML'}
        record['sha256']={m:hashlib.sha256((out/('GUARD'+m+'.EXE')).read_bytes()).hexdigest() for m in 'SCML'}
        for value in record['logs'].values():print(value.strip())
    except Exception as error:
        record.update(status='failed',error=str(error))
        raise
    finally:
        (out/'manifest.json').write_text(json.dumps(record,indent=2)+'\n')
if __name__=='__main__':main()
