"""Developer proof of the actual DOS sparse patch/header representation."""
from pathlib import Path
import json,re
import numpy as np
import trace_tandy_logo as art

ROOT=Path(__file__).resolve().parents[1]
text=(ROOT/'examples/smsboot/TRACE_RT.H').read_text()
def array(name):
    body=re.search(r'\b'+name+r'\[\]\s*=\s*\{(.*?)\}',text,re.S).group(1)
    return [int(n) for n in re.findall(r'(\d+)U',body)]
data=array('trace_initial');patches=array('trace_patches')
offsets=array('trace_patch_offsets');counts=array('trace_patch_counts')
times=array('trace_deadlines')
assert len(times)==52 and times[-1]==421
assert all(times[i]==art.deadline(i) for i in range(52))
_,blue,red,_=art.source_masks();birth,_=art.arrival_map(blue)
masks=art.make_masks(blue,red,birth)
vram=bytearray([255])*32768
linear=[]
for count,value in zip(data[::2],data[1::2]):linear.extend([value]*count)
assert len(linear)==19584
for y in range(136):
    off=((y+32)&3)*8192+((y+32)>>2)*160+8
    vram[off:off+144]=bytes(linear[y*144:(y+1)*144])
def pixels():
    rows=[]
    for y in range(136):
        off=((y+32)&3)*8192+((y+32)>>2)*160+8;row=[]
        for v in vram[off:off+144]:row.extend((v>>4,v&15))
        rows.append(row)
    return np.array(rows,dtype=np.uint8)
assert np.array_equal(pixels(),masks[0])
for phase in range(3):
    palette=[1]*14+[15,15]
    for pass_index in (phase*2,phase*2+1):
        if pass_index&1:
            assert not np.any((pixels()>=1)&(pixels()<=13))
            palette[1:14]=[15]*13
        start=offsets[pass_index]
        for i in range(counts[pass_index]):
            a,b,value=patches[start+3*i:start+3*i+3];addr=a|(b<<8)
            assert addr<32768
            before=vram[addr]
            assert palette[before>>4]==palette[value>>4]
            assert palette[before&15]==palette[value&15]
            vram[addr]=value
    assert np.array_equal(pixels(),masks[phase+1])
print(json.dumps({'runtime_patch_roundtrip':'pass','per_byte_RGB_invariance':'pass',
                  'patch_writes':sum(counts),'phase_deadline_windows_ticks':
                  [times[13*p]-times[13*p-1] for p in range(1,4)],
                  'trace_end_tick':421,'tagline_tick':481}))
