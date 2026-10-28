"""Standalone pure C89 merger checks, also usable against a Windows mirror."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, sys
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--out',type=Path,required=True)
a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
source=a.source.resolve();test=Path(__file__).with_suffix('.c')
cc=[sys.executable,'C:/Users/chad/Documents/Codex/toolchains/gcc-ucrt64.py','--'] if os.name=='nt' else ['cc']
env=dict(os.environ)
if os.name=='nt':env['PATH']='C:/msys64/ucrt64/bin;'+env.get('PATH','')
cmd=cc+['-x','c','-std=c89','-pedantic-errors','-Wall','-Wextra','-Werror','-I',str(source),str(source/'FXMERGE.C'),str(test),'-o',str(out/'fxmerge.exe')]
subprocess.run(cmd,check=True,env=env,timeout=30)
r=subprocess.run([str(out/'fxmerge.exe')],check=True,capture_output=True,text=True,env=env,timeout=10)
(out/'RESULT.JSON').write_text(json.dumps(dict(status='PASS',result=r.stdout.strip(),sources={str(s):hashlib.sha256(s.read_bytes()).hexdigest() for s in (source/'FXMERGE.C',source/'FXMERGE.H',test)}),indent=2))
print(r.stdout.strip())
